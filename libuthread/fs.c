#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define _UTHREAD_PRIVATE
#include "disk.h"
#include "fs.h"


// Very nicely display "Function Source of error: the error message"
#define fs_error(fmt, ...) \
	fprintf(stderr, "%s: ERROR-"fmt"\n", __func__, ##__VA_ARGS__)

#define EOC 0xFFFF
#define EMPTY 0


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
    char     signature[8];
    uint16_t numBlocks;
    uint16_t rootDirInd;
    uint16_t dataInd;
    uint16_t numDataBlocks;
    uint8_t  numFAT; 
    uint8_t  unused[4079];
} __attribute__((packed));

struct FAT_t {
	uint16_t words;
};

struct rootdirectory_t {
	char     filename[FS_FILENAME_LEN];
	uint32_t fileSize;
	uint16_t dataBlockInd;
	uint8_t  unused[10];
} __attribute__((packed));

struct file_descriptor_t {
    bool   is_used;       
    int    file_index;              
    size_t offset;  
	char   file_name[FS_FILENAME_LEN];
};


struct superblock_t      *mySuperblock;
struct rootdirectory_t   *myRootDir;
struct FAT_t             *myFAT;
struct file_descriptor_t fd_table[FS_OPEN_MAX_COUNT]; 

// private API
bool  error_free(const char *filename);
int  locate_file(const char* file_name);
bool is_open(const char* file_name);
int  locate_avail_fd();
int  get_num_FAT_free_blocks();
int  count_num_open_dir();
int  go_to_cur_fat_block(int cur_fat_index, int iter_amount);


/* Makes the file system contained in the specified virtual disk "ready to be used" */
int fs_mount(const char *diskname) {

	mySuperblock = malloc(BLOCK_SIZE);

	// open disk dd
	if(block_disk_open(diskname) < 0){
		fs_error("failure to open virtual disk \n");
		return -1;
	}
	
	// initialize data onto local super block 
	if(block_read(0, (void*)mySuperblock) < 0){
		fs_error( "failure to read from block \n");
		return -1;
	}
	// check for correct signature
	if(strncmp(mySuperblock->signature, "ECS150FS", 8) != 0){
		fs_error( "invalid disk signature \n");
		return -1;
	}
	// check for correct number of blocks on disk
	if(mySuperblock->numBlocks != block_disk_count()) {
		fs_error("incorrect block disk count \n");
		return -1;
	}

	// initialize data onto local FAT blocks
	myFAT = malloc(mySuperblock->numFAT * BLOCK_SIZE);
	for(int i = 0; i < mySuperblock->numFAT; i++) {
		// read each fat block in the disk starting at position 1
		if(block_read(i + 1, (void*)myFAT + (i * BLOCK_SIZE)) < 0) {
			fs_error("failure to read from block \n");
			return -1;
		}
	}

	// initialize data onto local root directory block
	myRootDir = malloc(sizeof(struct rootdirectory_t) * FS_FILE_MAX_COUNT);
	// read the root directory block in the disk starting after the last FAT block
	if(block_read(mySuperblock->numFAT + 1, (void*)myRootDir) < 0) { 
		fs_error("failure to read from block \n");
		return -1;
	}
	
	// initialize file descriptors 
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].is_used = false;
	}
        

	return 0;
}


/* Makes sure that the virtual disk is properly closed and that all the internal data structures of the FS layer are properly cleaned. */
int fs_umount(void) {

	if(!mySuperblock){
		fs_error("No disk available to unmount\n");
		return -1;
	}

	if(block_write(0, (void*)mySuperblock) < 0) {
		fs_error("failure to write to block \n");
		return -1;
	}

	for(int i = 0; i < mySuperblock->numFAT; i++) {
		if(block_write(i + 1, (void*)myFAT + (i * BLOCK_SIZE)) < 0) {
			fs_error("failure to write to block \n");
			return -1;
		}
	}

	if(block_write(mySuperblock->numFAT + 1, (void*)myRootDir) < 0) {
		fs_error("failure to write to block \n");
			return -1;
	}

	free(mySuperblock);
	free(myRootDir);
	free(myFAT);

	// reset file descriptors
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].offset = 0;
		fd_table[i].is_used = false;
		fd_table[i].file_index = -1;
		memset(fd_table[i].file_name, 0, FS_FILENAME_LEN);
    }

	block_disk_close();
	return 0;
}


/* Display some information about the currently mounted file system. */
int fs_info(void) {

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", mySuperblock->numBlocks);
	printf("fat_blk_count=%d\n", mySuperblock->numFAT);
	printf("rdir_blk=%d\n", mySuperblock->numFAT + 1);
	printf("data_blk=%d\n", mySuperblock->numFAT + 2);
	printf("data_blk_count=%d\n", mySuperblock->numDataBlocks);
	printf("fat_free_ratio=%d/%d\n", get_num_FAT_free_blocks(), mySuperblock->numDataBlocks);
	printf("rdir_free_ratio=%d/128\n", count_num_open_dir());

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

	// // perform error checking first 
	// if(error_free(filename) == false) {
	// 	fs_error("error associated with filename");
	// 	return -1;
	// }

	// // finds first available empty file
	// for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
	// 	if(myRootDir[i].filename[0] == EMPTY) {	

	// 		// initialize file data 
	// 		strcpy(myRootDir[i].filename, filename);
	// 		myRootDir[i].fileSize     = 0;
	// 		myRootDir[i].dataBlockInd = EOC;

	// 		// for debugging purposes
	// 		printf("Created file: '%s' (%d/%d bytes)\n", myRootDir[i].filename, myRootDir[i].fileSize, myRootDir[i].fileSize);
	// 		return 0;
	// 	}
	// }

	// fs_error("all root directory mappings are full");
	// return -1;

	// perform error checking first 
	if(error_free(filename) == false) {
		fs_error("error associated with filename");
		return -1;
	}

	// finds first available empty file
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(myRootDir[i].filename[0] == 0x00) {	

			// initialize file data 
			strcpy(myRootDir[i].filename, filename);
			myRootDir[i].fileSize     = 0;
			myRootDir[i].dataBlockInd = EOC;

			// for debugging purposes
			printf("Created file: '%s' (%d/%d bytes)\n", myRootDir[i].filename, myRootDir[i].fileSize, myRootDir[i].fileSize);
			return 0;
		}
	}
	return -1;
}

/*
Remove File:
	1. Empty file entry and all its datablocks associated with file contents from FAT.
	2. Free associated data blocks

*/

int fs_delete(const char *filename) {
	
	if (is_open(filename)) {
		fs_error("file currently open");
		return -1;
	}

	int file_index = locate_file(filename);
	struct rootdirectory_t* the_dir = &myRootDir[file_index]; 
	int frst_dta_blk_i = the_dir->dataBlockInd;

	
	while (frst_dta_blk_i != EOC) {
		uint16_t tmp = myFAT[frst_dta_blk_i].words;
		myFAT[frst_dta_blk_i].words = EMPTY;
		frst_dta_blk_i = tmp;
	}

	// reset file to blank slate
	memset(the_dir->filename, 0, FS_FILENAME_LEN);
	the_dir->fileSize = 0;

	return 0;

}

int fs_ls(void) {

	printf("FS Ls:\n");
	// finds first available file block in root dir 
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(myRootDir[i].filename[0] != 0x00) {
			printf("file: %s, size: %d, ", myRootDir[i].filename, myRootDir[i].fileSize);
			printf("data_blk: %d\n", myRootDir[i].dataBlockInd);
														
		}
	}	

	return 0;
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
        fs_error("file @[%s] doesnt exist\n", filename);
        return -1;
    } 

    int fd = locate_avail_fd();
    if (fd == -1){
		fs_error("max file descriptors already allocated\n");
        return -1;
    }

	fd_table[fd].is_used    = true;
	fd_table[fd].file_index = file_index;
	fd_table[fd].offset     = 0;
	
	strcpy(fd_table[fd].file_name, filename); 

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

    if(fd >= FS_OPEN_MAX_COUNT || fd < 0 || fd_table[fd].is_used == 0) {
		fs_error("invalid file descriptor supplied \n");
        return -1;
    }

    struct file_descriptor_t *fd_obj = &fd_table[fd];

    int file_index = locate_file(fd_obj->file_name);
    if(file_index == -1) { 
        fs_error("file @[%s] doesnt exist\n", fd_obj->file_name);
        return -1;
    } 

    fd_obj->is_used = false;

	return 0;
}

/*
	1. Return the size of the file corresponding to the specified file descriptor.
*/
int fs_stat(int fd) {
    if(fd >= FS_OPEN_MAX_COUNT || fd < 0 || fd_table[fd].is_used == false) {
		fs_error("invalid file descriptor supplied \n");
        return -1;
    }

    struct file_descriptor_t *fd_obj = &fd_table[fd];

    int file_index = locate_file(fd_obj->file_name);
    if(file_index == -1) { 
        fs_error("file @[%s] doesnt exist\n", fd_obj->file_name);
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
        fs_error("file @[%s] doesnt exist\n", fd_obj->file_name);
        return -1;
    } 

	int32_t file_size = fs_stat(fd);
	
	if (offset < 0 || offset > file_size) {
        fs_error("file @[%s] is out of bounds \n", fd_obj->file_name);
        return -1;
	} else if (fd_table[fd].is_used == false) {
        fs_error("invalid file descriptor [%s] \n", fd_obj->file_name);
        return -1;
	} 

	fd_table[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count) {
	if (count <= 0) {
        fs_error("request nbytes amount is trivial" );
        return -1;
	} else if (fd <= -1 || fd >= 4) {
        fs_error("invalid file descriptor [%d] \n", fd);
        return -1;
	} else if (get_num_FAT_free_blocks() == EMPTY) {
        fs_error("no free entries to write to");
        return -1;
	}

/*
	char *file_name = fd_table[fd].file_name;
	int file_index = locate_file(file_name);
	int offset = fd_table[fd].offset;

	struct rootdirectory_t *the_dir = &myRootDir[file_index];
	int cur_fileSize = the_dir->fileSize;

	int new_blocks;
	if (offset + count > cur_fileSize) {
		new_blocks = ((offset + count) / BLOCK_SIZE) - (cur_fileSize / BLOCK_SIZE);
	} else {
		new_blocks = 0;
	}

	int num_blocks = ((count + (offset % BLOCK_SIZE)) / BLOCK_SIZE) + 1;
	int cur_block = offset/BLOCK_SIZE;
	int fat_block = the_dir->dataBlockInd;

	char *to_write = (char *)buf;
	char buffer[BLOCK_SIZE];
	
	int amount_left = count;
	int left_shift;
	int my_count = 0;
	int loc = offset % BLOCK_SIZE;

	fat_block = go_to_cur_fat_block(fat_block, cur_block);
	
*/
	return 0;


}

/*
Read a File:
	1. Error check that the amount to be read is > 0, and that the
	   the file descriptor is valid.
*/
int fs_read(int fd, void *buf, size_t count) {
	
	// error check 
    if(fd_table[fd].is_used == false || 
	   fd >= FS_OPEN_MAX_COUNT) {
		fs_error("invalid file descriptor [%d]", fd);
        return -1;
    } else if (count <= 0) {
		fs_error("request nbyte amount is trivial");
		return -1;
	} 

	// gather nessessary information 
	char *file_name = fd_table[fd].file_name;
	int file_index = locate_file(file_name);
	size_t offset = fd_table[fd].offset;
	
	struct rootdirectory_t *the_dir = &myRootDir[file_index];


	// check if offset of file exceeds the file_size
	int amount_to_read = 0;
	if (offset + count > the_dir->fileSize) 
		amount_to_read = abs(the_dir->fileSize - offset);
	else amount_to_read = count;

	char *read_buf = (char *)buf;
	int16_t FAT_iter = the_dir->dataBlockInd;
	//size_t num_blocks = ceil(amount_to_read / BLOCK_SIZE);
	size_t num_blocks = (amount_to_read / BLOCK_SIZE) + 1;
	
	// block level
	int cur_block = offset / BLOCK_SIZE; 

	// byte level
	int loc = offset % BLOCK_SIZE;
	char bounce_buff[BLOCK_SIZE];
		
	// go to correct current block in fat entry 		
	for (int i = 0; i < cur_block; i++) {
		FAT_iter = myFAT[FAT_iter].words;
	}

	// read through the number of blocks it contains
	int left_shift = 0;
	int cont = 0;
	for (int i = 0; i < num_blocks; i++) {
		if (loc + amount_to_read > BLOCK_SIZE) {
			left_shift = BLOCK_SIZE - loc;
		} else {
			left_shift = amount_to_read;
		}

		// read file contents 
		block_read(FAT_iter + mySuperblock->rootDirInd + 1, (void*)bounce_buff);
		memcpy(read_buf, bounce_buff + loc, left_shift);

		// position array to left block 
		cont += left_shift;
		read_buf += left_shift;

		// next block starts at the top
		loc = 0;

		// next 
		FAT_iter = myFAT[FAT_iter].words;

		// reduce the amount to read by the amount that was read 
		amount_to_read -= left_shift;
	}

	fd_table[fd].offset += cont;
	return cont;
}


/*
Locate Existing File
	1. Return the first filename that matches the search,
	   and is in use (contains data).
*/
int locate_file(const char* file_name) {
	int i;
    for(i = 0; i < FS_FILE_MAX_COUNT; i++) 
        if(strncmp(myRootDir[i].filename, file_name, FS_FILENAME_LEN) == 0 &&  
			      myRootDir[i].filename != 0x00) 
            return i;  
    return -1;      
}


int locate_avail_fd() {
	int i;
	for(i = 0; i < FS_OPEN_MAX_COUNT; i++) 
        if(fd_table[i].is_used == false) 
			return i; 
    return -1;
}


/*
 * Perform Error Checking 
 * 1) Check if file length>16
 * 2) Check if file already exists 
 * 3) Check if root directory has max number of files 
*/

bool error_free(const char *filename){

	// get size 
	int size = strlen(filename);
	if(size > FS_FILENAME_LEN){
		fs_error("File name is longer than FS_FILE_MAX_COUNT\n");
		return false;
	}

	// check if file already exists 
	int same_char = 0;
	int files_in_rootdir = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		for(int j = 0; j < size; j ++){
			if(myRootDir[i].filename[j] == filename[j])
				same_char++;
		}
		if(myRootDir[i].filename[0] != EMPTY)
			files_in_rootdir++;
	}
	// File already exists
	if(same_char == size){
		fs_error("file @[%s] already exists\n", filename);
		return false;
	}
		

	// if there are 128 files in rootdirectory 
	if(files_in_rootdir == FS_FILE_MAX_COUNT){
		fs_error("All files in rootdirectory are taken\n");
		return false;
	}
		
	return true;
}

bool is_open(const char* filename)
{
	int file_index = locate_file(filename);

	if (file_index == -1) {
		fs_error("file @[%s] doesnt exist\n", filename);
        return true;
	}

	struct rootdirectory_t* the_dir = &myRootDir[file_index]; 
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(strncmp(the_dir->filename, fd_table[i].file_name, FS_FILENAME_LEN) == 0 
		   && fd_table[i].is_used) {
			fs_error("cannot remove file @[%s] as it is currently open\n", filename);
			return true;
		}
	}

	return false;
}

int get_num_FAT_free_blocks()
{
	int count = 0;
	for (int i = 1; i < mySuperblock->numDataBlocks; i++) {
		if (myFAT[i].words == EMPTY) count++;
	}
	return count;
}

int count_num_open_dir(){

	int i, count = 0;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(myRootDir[i].filename[0] == EMPTY)
			count++;
	}
	return count;
}

int go_to_cur_fat_block(int cur_fat_index, int iter_amount)
{
	for (int i = 0; i < iter_amount; i++) {
		if (cur_fat_index == EOC) {
			fs_error("attempted to exceed end of file chain");
			return -1;
		}
		cur_fat_index = myFAT[cur_fat_index].words;
	}
	return cur_fat_index;
}