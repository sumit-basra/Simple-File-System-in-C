#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h> // ceil

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
    char    signature[8];
    int16_t numBlocks;
    int16_t rootDirInd;
    int16_t dataInd;
    int16_t numDataBlocks;
    int8_t  numFAT; 
    uint8_t unused[4079];
} __attribute__((packed));


//struct FAT_t {
	uint16_t *FAT_entry;
//} __attribute__((packed));			// if this is all you have for fat, then just make it  uint16_t *myFat;

struct rootdirectory_t {
	char    filename[FS_FILENAME_LEN];	// don't use chars, cast as char when needed	
	int32_t fileSize;
	int16_t dataBlockInd;
	uint8_t unused[10];
} __attribute__((packed));


struct file_descriptor_t {
    bool   is_used;       
    int    file_index;              
    size_t offset;  
	char   file_name[FS_FILENAME_LEN]; // we may not need this but it make it easier
	//struct rootdirectory_t *dir_ptr;   // we may not need this      
};


struct superblock_t      *mySuperblock;
struct rootdirectory_t   *myRootDir;
//struct FAT_t             *myFAT;
struct file_descriptor_t fd_table[FS_OPEN_MAX_COUNT]; 

// private API
bool error_free(const char *filename);
int locate_file(const char* file_name);
bool is_open(const char* file_name);
int locate_avail_fd();
int get_num_FAT_free_blocks();

// return -1 if there is a next block to read 
// or other wise >= 0 for the new fd offset pos to be set
int read_next_block_to_buffer(void *buf, int **cur_buf_pos);

// Makes the file system contained in the specified virtual disk "ready to be used
int fs_mount(const char *diskname) {

	mySuperblock = malloc(sizeof(struct superblock_t));

	// open disk 
	if(block_disk_open(diskname) < 0) {
		fs_error("failure to open virtual disk \n");
		return -1;
	}
	
	// initialize data onto local super block 
	if(block_read(0, mySuperblock) < 0) {
		fs_error( "failure to read from block \n");
		return -1;
	}

	if(strncmp(mySuperblock->signature, "ECS150FS", 8) != 0) {
		fs_error( "invalid disk signature \n");
		return -1;
	}

	if(mySuperblock->numBlocks != block_disk_count()) {
		fs_error("incorrect block disk count \n");
		return -1;
	}


	// the size of the FAT (in terms of blocks)
	//int num_FAT_blocks = ceil(double((mySuperblock->numDataBlocks * 2)) / BLOCK_SIZE); 
	//int num_FAT_blocks = mySuperblock->numFAT;
	//int num_FAT_blocks = (mySuperblock->numDataBlocks * 2) / BLOCK_SIZE;
	//if(num_FAT_blocks == 0)
	//	num_FAT_blocks =1;
	
	static uint8_t buffer[BLOCK_SIZE];
	//uint16_t *temp;
	FAT_entry = malloc(2* mySuperblock->numDataBlocks);		// 2 bytes * number of datablocks
	int keep_track = 0;
	for(int i = 1; i < mySuperblock->rootDirInd; i++) {
		// read each fat block in the disk starting at position 1

		if(block_read(i, (void *) buffer) < 0) {
			fs_error("failure to read from block \n");
			return -1;
		}
		
		for(int j = 0; j*2 < BLOCK_SIZE; j++){
			//temp = (((uint16_t*) buffer) + j);
			FAT_entry[keep_track] = *(((uint16_t*) buffer) + j);                                        
			keep_track++;
		}
		}

	// root directory creation
	myRootDir = (struct rootdirectory_t*) malloc(sizeof(struct rootdirectory_t) * FS_FILE_MAX_COUNT);
	// FAT_blocks is size of fat - Root Directory starts here.
	if(block_read(mySuperblock->numFAT + 1, myRootDir) < 0) { 
		fs_error("failure to read from block \n");
		return -1;
	}
	
	// initialize file descriptors 
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].is_used = false;
		//fd_table[i].dir_ptr = NULL;
	}
        
	return 0;
}


/* Makes sure that the virtual disk is properly closed and that all the internal data structures of the FS layer are properly cleaned. */
int fs_umount(void) {

	if(!mySuperblock) {
		fs_error("No disk available to unmount\n");	
		return -1;
	}

	// write super block
	if(block_write(0, mySuperblock) < 0) {
		fs_error("failure to write to block \n");
		return -1;
	}

	// write FAT
	for(int i = 1; i <= mySuperblock->numFAT; i++) {
		if(block_write(i, FAT_entry + (i * BLOCK_SIZE)) < 0) {
			fs_error("failure to write to block \n");
			return -1;
		}
	}

	// write root dir 
	if(block_write(mySuperblock->numFAT + 1, FAT_entry) < 0){
		fs_error("failure to write to block \n");
		return -1;
	}

	free(mySuperblock);
	free(myRootDir);
	free(FAT_entry);

	// reset file descriptors
    for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		fd_table[i].offset = 0;
		fd_table[i].is_used = false;
		fd_table[i].file_index = -1;
		strcpy(fd_table[i].file_name, "\0");
    }

	return block_disk_close();
}

/* 
 * to create a test file system call ./fs.x make test.fs 8192 to 
 * make a virtual disk with 8192 datablocks) 
 */

/* Display some information about the currently mounted file system. */
int fs_info(void) {

    // the size of the FAT (in terms of blocks)
    // we can use the superblock var = numFAT. 
	//int FAT_blocks = ((mySuperblock->numDataBlocks) * 2)/BLOCK_SIZE;  // use ceil to round up
	//int FAT_blocks = mySuperblock->numFAT;
	//if(FAT_blocks == 0)
	//	FAT_blocks = 1;

	int i, count = 0;

	for(i = 0; i < 128; i++) {
		//printf("%c\n", myRootDir[i].filename[0]);
		if(myRootDir[i].filename[0] == EMPTY)
			count++;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", mySuperblock->numBlocks);
	printf("fat_blk_count=%d\n", mySuperblock->numFAT);
	printf("rdir_blk=%d\n", mySuperblock->numFAT + 1);
	printf("data_blk=%d\n", mySuperblock->numFAT + 2);
	printf("data_blk_count=%d\n", mySuperblock->numDataBlocks);
	printf("fat_free_ratio=%d/%d\n", get_num_FAT_free_blocks(), mySuperblock->numDataBlocks);
	printf("rdir_free_ratio=%d/128\n", count);

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

	// perform error checking first 
	if(error_free(filename) == false)
		return -1;


	// finds first available empty file
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(myRootDir[i].filename[0] == EMPTY) {	

			// initialize file data 
			strcpy(myRootDir[i].filename, filename);
			//printf("Filename: %s\n", myRootDir[i].filename);
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
	1. Empty file entry and and confirm some error checking.
	2. Free associated data blocks
*/
int fs_delete(const char *filename) {

	if (!is_open(filename)) return -1;

	/*
	Free associated blocks 
		1. Get the starting data index block from the file (FAT Table)
		2. Calculate the number of block it has, given its size
		3. Read in the first data blocks from the super block (there are two of them)
		4. Iterate through each block that is associated with the file 
			4.1 If the blocks index is within the bounds of the block, set the buffer to 
			    at that location to null signal a free entry.
			4.2 If the block is otherwise out of bounds of the block, set its offset to null.
			4.3 Find the next block
		5. Write to block with new changes 
		6. Reset file attribute data
	*/

	int file_index = locate_file(filename);
	struct rootdirectory_t* the_dir = &myRootDir[file_index]; 
	int frst_dta_blk_i = the_dir->dataBlockInd;
	int num_blocks = the_dir->fileSize / BLOCK_SIZE;

	char buf1[BLOCK_SIZE] = "";
	char buf2[BLOCK_SIZE] = "";

	block_read(mySuperblock->dataInd, buf1);		
	block_read(mySuperblock->dataInd + 1, buf2);	

	for (int i = 0; i < num_blocks; i++) {
		if (frst_dta_blk_i < BLOCK_SIZE)
			buf1[frst_dta_blk_i] = '\0';
		else buf2[frst_dta_blk_i - BLOCK_SIZE] = '\0';
		
		// todo: need to find a way to read in the next 
		// block for larger files 
		
		// the information here is stored in that FAT fd_table 
		// directory so those are the only blocks we need to reference
	}

	myRootDir[file_index].dataBlockInd = -1;

	block_write(mySuperblock->dataInd, buf1);
	block_write(mySuperblock->dataInd + 1, buf2);

	// reset file to blank slate
	strcpy(the_dir->filename, "\0");
	the_dir->fileSize = 0;

	return 0;

}

/*
List all the existing files:
	1. A file is assummed to be existing if the first 
	   byte of its filename is non-zero. 
*/
int fs_ls(void) {

	printf("FS Ls:\n");
	// finds first available file block in root dir 
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		//printf("Filename: %s\n", myRootDir[i].filename);
		if(myRootDir[i].filename[0] != EMPTY) {
			printf("file: %s, size: %d, ", myRootDir[i].filename, myRootDir[i].fileSize);
			printf("data_blk: %d\n", (myRootDir + i)->dataBlockInd);									
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

	return 0;
	/* TODO: PART 3 - Phase 4 */
}

/*
Read a File: (will clean up later)
	1. Error check that the amount to be read is > 0, and that the
	   the file descriptor is valid.
*/
int fs_read(int fd, void *buf, size_t count) {
	return 0;
}
/* commenting b/c buggy
int fs_read(int fd, void *buf, size_t count) {
	
    if(fd_table[fd].is_used == false || count <= 0) {
		fs_error("fs_read()\t error: invalid file descriptor [%d] or count <= 0\n", fd);
        return -1;
    }

	// gather nessessary information 
	char *file_name = fd_table[fd].file_name;
	size_t cur_offset = fd_table[fd].offset;
	int file_index = locate_file(file_name);
	
	struct rootdirectory_t *the_dir = &myRootDir[file_index];
	int frst_dta_blk_i = the_dir->dataBlockInd;
	
	int cur_num_blocks_read = 0;

	// start with initial block i
	// get to the correct offset by continuing 
	// to read past block by block 
	while (cur_offset >= BLOCK_SIZE) {
		// todo: implement find_next_block for larger files
		//frst_dta_blk_i = find_next_block(frst_dta_blk_i, file_index);
		cur_num_blocks_read++;
		cur_offset -= BLOCK_SIZE;
	}

	// the finally do a read once your at the proper block that contains file
	char *temp_blk[BLOCK_SIZE] = "";
	block_read(frst_dta_blk_i, temp_blk);

	// first go to the correct block, and then seek the correct offset in the block 
	int iter = 0;
	int next_db_instance = frst_dta_blk_i; // start us off
	// starting reading starting from offset of the correct block 
	// a block offset can start anywhere 
	for (int off_s = cur_offset; i < BLOCK_SIZE; i++) {
		// store the contents into the buffer piece by piece from first block
		// ill probabely modularize this part into a function later
		buf[iter++] = temp_blk[off_s];

		// if we exceed the number of bytes (count) that is required to write in the block
		// that means that the contents of the file are only within that single block
		// so we can stop here after 
		if (iter >= count) {
			fd_table[fd].offset += iter;

			// from the header file: 
			//The file offset of the file descriptor is implicitly incremented by the number of bytes that were actually read. why?
			
			// return the number of bytes that were read
			return iter;
		}
	}

	// know we know that the block offset starts from 0 (next new block)
	// if we are here there are more blocks to read
	// read block by block

	// continue to read requested amount of bytes, but dont go overboard 
	int max_blocks = mySuperblock->numDataBlocks;
	cur_num_blocks_read++;
	while (iter <= count && cur_num_blocks_read <= max_blocks) {
		// todo: implement find_next_block for larger files
		//next_db_instance = find_next_block(next_db_instance, file_index);
		
		// reset buffer block
		strcpy(temp_blk, "");
		block_read(frst_dta_blk_i, temp_blk);
		if (read_next_block_to_buffer(buf, &iter, count, fd) = 0) { // exit status
			fd_table[fd].offset += iter;
			return iter;
		} 
		cur_num_blocks_read++;
	}

	// otherwise exceeded bounds or in on the very edge of a block 
	fd_table[fd].offset += iter;
	return iter;
}


int read_next_block_to_buffer(void *buf, int *cur_buf_pos, int next_block_index, int nbytes, int fd)
{
	// reset new block
	char *temp_blk[BLOCK_SIZE] = "";
	block_read(frst_dta_blk_i, temp_blk);

	// read full block 
	for (int cur_block_i = 0 ; cur_block_i < BLOCK_SIZE ; i++ ) {
		// add to buffer 
		buf[*(cur_buf_pos)++] = temp_blk[cur_block_i];

		if (cur_buf_pos >= nbytes) {
			fd_table[fd].offset += iter;
			return 0; // finished 
		}
	}
	return -1; // not finished 
}
*/

/*
Locate Existing File
	1. Return the first filename that matches the search,
	   and is in use (contains data).
*/
int locate_file(const char* file_name) {
	int i;
    for(i = 0; i < FS_FILE_MAX_COUNT; i++) 
        if(strncmp((myRootDir + i)->filename, file_name, FS_FILENAME_LEN) == 0 &&  
			      (myRootDir + i)->filename != EMPTY) 
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
	for (int i = 0; i < mySuperblock->numDataBlocks; i++) {
		if (FAT_entry[i] == EMPTY) count++;
	}
	return count;

}

