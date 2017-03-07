#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define _UTHREAD_PRIVATE
#include "disk.h"
#include "fs.h"

/* 
 * 
 * Superblock:
 * The superblock is the first block of the file system. Its internal format is:
 * Offset	Length (bytes)	Description
 * 0x00		8-				Signature (must be equal to "ECS150FS")
 * 0x08		2-				Total amount of blocks of virtual disk
 * 0x0A		2-				Root directory block index
 * 0x0C		2-				Data block start index
 * 0x0E		2				Amount of data blocks
 * 0x10		1				Number of blocks for FAT
 * 0x11		4079			Unused/Padding
 *
 *
 *
 * FAT:
 * The FAT is a flat array, possibly spanning several blocks, which entries are composed of 16-bit unsigned words. 
 * There are as many entries as data *blocks in the disk.
 *
 * Root Directory:
 * Offset	Length (bytes)	Description
 * 0x00		16				Filename (including NULL character)
 * 0x10		4				Size of the file (in bytes)
 * 0x14		2				Index of the first data block
 * 0x16		10				Unused/Padding
 *
 */

typedef enum { false, true } bool;

struct superblock_t {
    char    signature[8];
    int16_t numBlocks;
    int16_t rootDirInd;
    int16_t dataInd;
    int16_t numDataBlocks;
    int8_t  numFAT; 
    uint8_t unused[4079];
} __attribute__((packed));


struct FAT_t {
	uint16_t words;
} __attribute__((packed));

struct rootdirectory_t {
	char    filename[FS_FILENAME_LEN];
	int32_t fileSize;
	int16_t dataBlockInd;
	uint8_t num_fd_pointers;
	uint8_t unused[9];
} __attribute__((packed));


struct file_descriptor_t
{
    bool is_used;       
    int  file_index;              
    size_t  offset;  
	char file_name[FS_FILENAME_LEN];
	struct rootdirectory_t *dir_ptr;         
};


struct superblock_t      *mySuperblock;
struct rootdirectory_t   *myRootDir;
struct FAT_t             *myFAT;
struct file_descriptor_t fd_table[FS_OPEN_MAX_COUNT]; 

// private API
int locate_file(const char* file_name);
int locate_avail_fd();

/* Makes the file system contained in the specified virtual disk "ready to be used" */
int fs_mount(const char *diskname) {

	mySuperblock = malloc(sizeof(struct superblock_t));

	// open disk 
	if(block_disk_open(diskname) < 0){
		fprintf(stderr, "fs_mount()\t error: failure to open virtual disk \n");
		return -1;
	}
	
	// initialize data onto local super block 
	if(block_read(0, mySuperblock) < 0){
		fprintf(stderr, "fs_mount()\t error: failure to read from block \n");
		return -1;
	}

	if(strncmp(mySuperblock->signature, "ECS150FS",8 ) !=0){
		fprintf(stderr, "fs_mount()\t error: invalid disk signature \n");
		return -1;
	}

	if(mySuperblock->numBlocks != block_disk_count()) {
		fprintf(stderr, "fs_mount()\t error: incorrect block disk count \n");
		return -1;
	}


	// the size of the FAT (in terms of blocks)
	int FAT_blocks = ((mySuperblock->numDataBlocks) * 2)/BLOCK_SIZE; 
	if(FAT_blocks == 0)
		FAT_blocks =1;

	myFAT = malloc(sizeof(struct FAT_t) * FAT_blocks);
	for(int i = 1; i <= FAT_blocks; i++) {
		// read each fat block in the disk starting at position 1
		if(block_read(i, myFAT + (i * BLOCK_SIZE)) < 0) {
			fprintf(stderr, "fs_mount()\t error: failure to read from block \n");
			return -1;
		}
	}

	// root directory creation
	myRootDir = malloc(sizeof(struct rootdirectory_t) * FS_FILE_MAX_COUNT);
	if(block_read(FAT_blocks + 1, myRootDir) < 0) { // FAT_blocks is size of fat - Root Directory starts here.
		fprintf(stderr, "fs_mount()\t error: failure to read from block \n");
		return -1;
	}
	
	// initialize file descriptors 
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].is_used = false;
		fd_table[i].dir_ptr = NULL;
	}
        

	return 0;
}


/* Makes sure that the virtual disk is properly closed and that all the internal data structures of the FS layer are properly cleaned. */
int fs_umount(void) {

	if(!mySuperblock){
		fprintf(stderr, "fs_umount()\t error: No disk available to unmount\n");
		return -1;
	}

	free(mySuperblock->signature);
	free(mySuperblock);
	free(myRootDir->filename);
	free(myRootDir);
	free(myFAT);

	// reset file descriptors
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].offset = 0;
		fd_table[i].is_used = false;
		fd_table[i].file_index = -1;
		strcpy(fd_table[i].file_name, "");
    }

	block_disk_close();
	return 0;
}

/* 
 * to create a test file system call ./fs.x make test.fs 8192 to 
 * make a virtual disk with 8192 datablocks) 
 */

/* Display some information about the currently mounted file system. */
int fs_info(void) {

    // the size of the FAT (in terms of blocks)
	int FAT_blocks = ((mySuperblock->numDataBlocks) * 2)/BLOCK_SIZE; 
	if(FAT_blocks == 0)
		FAT_blocks =1;

	int i, count = 0;

	for(i = 0; i < 128; i++) {
		if((myRootDir + i)->filename[0] == 0x00)
			count++;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n",mySuperblock->numBlocks);
	printf("fat_blk_count=%d\n",FAT_blocks);
	printf("rdir_blk=%d\n", FAT_blocks+1);
	printf("data_blk=%d\n",FAT_blocks+2 );
	printf("data_blk_count=%d\n", mySuperblock->numDataBlocks);
	printf("fat_free_ratio=%d/%d\n", (mySuperblock->numDataBlocks - FAT_blocks),mySuperblock->numDataBlocks );
	printf("rdir_free_ratio=%d/128\n",count);

	return 0;
}


/*
Create a new file:
	0. Make sure we don't duplicate files, by checking for existings.
	1. Find an empty entry in the root directory.
	2. The name needs to be set, and all other information needs to get reset.
		2.2 Intitially the size is 0 and pointer to first data block is FAT_EOC.
*/

int fs_create(const char *filename) {

	int file_index = locate_file(filename);

	if (file_index >= 0) {
		fprintf(stderr, "fs_create()\t error: file @[%s] already exists\n", filename);
        return -1;
	} 

	// finds first available empty file
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(myRootDir[i].filename[0] == 0x00) {
			myRootDir[i].dataBlockInd = myFAT[0].words;

			// initialize file data 
			strcpy(myRootDir[i].filename, filename);
			myRootDir[i].fileSize        = 0;
			myRootDir[i].dataBlockInd    = -1;
			myRootDir[i].num_fd_pointers = 0;
			return 0;
		}
	}

	fprintf(stderr, "fs_create()\t error: exceeded FS_FILE_MAX_COUNT.\n");
	return -1;
}

/*
Remove File:
	1. Empty file entry and all its datablocks associated with file contents from FAT.
	2. Free associated data blocks

*/
int fs_delete(const char *filename) {

	// confirm existing filename
	int file_index = locate_file(filename);

	if (file_index == -1) {
		fprintf(stderr, "fs_delete()\t error: file @[%s] doesnt exist\n", filename);
        return -1;
	}

	struct rootdirectory_t* the_file = &myRootDir[file_index]; 
	if (the_file->num_fd_pointers > 0) { 
		fprintf(stderr, "fs_delete()\t error: cannot remove file @[%s],\
						 as it is currently open\n", filename);
		return -1;
	}

	// reset file to blank slate
	strcpy(the_file->filename, "\0");
	the_file->fileSize        = 0;
	the_file->num_fd_pointers = 0;


	// free associated blocks 

	/*	
		what I think we should do:
		1. find starting index from fat table 
		2. place the contents of the files into a buffer that is of size BLOCK BLOCK_SIZE
		3. do a block read, and place the contents of the block into that buffer given (1)
		4. modify the contents somehow
		5. continue iterate through the blocks until null 
		6. then do a block write with the buffers you modified.

		just brainstorming
	*/

	return 0;
}

/*
List all the existing files:
	1. 
*/
int fs_ls(void) {
	
	return 0;
	/* TODO: PART 3 - Phase 2 */
}

/*
Open and return FD:
	1. Find the file
	2. Find an available file descriptor
		2.1 Mark the particular descriptor in_use, and remaining other properties
			2.1.1 Set offset or current reading position to 0
		2.2 Increment number of file scriptors to of requested file object
	3. Return file descriptor index, or other wise -1 on failure
*/
int fs_open(const char *filename) {

    int file_index = locate_file(filename);
    if(file_index == -1) { 
        fprintf(stderr, "fs_open()\t error: file @[%s] doesnt exist\n", filename);
        return -1;
    } 

    int fd = locate_avail_fd();
    if (fd == -1){
		fprintf(stderr, "fs_open()\t error: max file descriptors already allocated\n");
        return -1;
    }

	fd_table[file_index].is_used = true;
	fd_table[file_index].file_index = file_index;
	fd_table[file_index].offset = 0;
	strcpy(fd_table[file_index].file_name, filename); 

    myRootDir[file_index].num_fd_pointers++;
    return fd;

}

/*
Close FD object:
	1. Check that it is a valid FD
	2. Locate file descriptor object, given its index
	3. Locate its the associated filename of the fd and decrement its fd
	4. Mark FD as available for use
*/
int fs_close(int fd) {
    if(fd >= FS_OPEN_MAX_COUNT || fd < 0 || fd_table[fd].is_used == false) {
		fprintf(stderr, "fs_close()\t error: invalid file descriptor supplied \n");
        return -1;
    }

    struct file_descriptor_t *fd_obj = &fd_table[fd];

    int file_index = locate_file(fd_obj->file_name);
    if(file_index == -1) { 
        fprintf(stderr, "fs_close()\t error: file @[%s] doesnt exist\n", fd_obj->file_name);
        return -1;
    } 

    myRootDir[file_index].num_fd_pointers--;
    fd_obj->is_used = false;

	return 0;
}

/*
	1. Return the size of the file corresponding to the specified file descriptor.
*/
int fs_stat(int fd) {
    if(fd >= FS_OPEN_MAX_COUNT || fd < 0 || fd_table[fd].is_used == false) {
		fprintf(stderr, "fs_stat()\t error: invalid file descriptor supplied \n");
        return -1;
    }

    struct file_descriptor_t *fd_obj = &fd_table[fd];

    int file_index = locate_file(fd_obj->file_name);
    if(file_index == -1) { 
        fprintf(stderr, "fs_close()\t error: file @[%s] doesnt exist\n", fd_obj->file_name);
        return -1;
    } 

	return myRootDir[file_index].fileSize;
}

/*
Move supplied fd to supplied offset
	1. Make sure the offset is valid: cannot be less than zero, nor can 
	   it exceed the size of the file itself.
*/
int fs_lseek(int fd, size_t offset) {
	struct file_descriptor_t *fd_obj = &fd_table[fd];
    int file_index = locate_file(fd_obj->file_name);
    if(file_index == -1) { 
        fprintf(stderr, "fs_lseek()\t error: file @[%s] doesnt exist\n", fd_obj->file_name);
        return -1;
    } 

	int32_t file_size = fs_stat(fd);
	
	if (offset < 0 || offset > file_size) {
        fprintf(stderr, "fs_lseek()\t error: file @[%s] is out of bounds \n", fd_obj->file_name);
        return -1;
	} else if (fd_table[fd].is_used == false) {
        fprintf(stderr, "fs_lseek()\t error: invalid file descriptor [%s] \n", fd_obj->file_name);
        return -1;
	} 

	fd_table[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count) {

	return 0;
	/* TODO: PART 3 - Phase 4 */
}

int fs_read(int fd, void *buf, size_t count) {

	return 0;
	/* TODO: PART 3 - Phase 4 */
}


/*
Locate Existing File
	1. Return the first filename that matches the search,
	   and is in use (contains data).
*/
int locate_file(const char* file_name)
{
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) 
        if(strcmp(myRootDir[i].filename, file_name) == 0 && 
			      myRootDir[i].filename == 0x00) 
            return i;  
    return -1;      
}

int locate_avail_fd()
{
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) 
        if(fd_table[i].is_used == false) 
			return i; 
    return -1;
}
