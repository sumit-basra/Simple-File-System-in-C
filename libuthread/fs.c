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
    char signature[8];
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
	char filename[FS_FILENAME_LEN];
	int32_t fileSize;
	int16_t dataBlockInd;
	uint8_t unused[10];
} __attribute__((packed));


struct file_descriptor_t
{
    bool is_used;       
    char file_index;              
    int offset;           
};


struct superblock_t      *mySuperblock;
struct rootdirectory_t   *myRootDir;
struct FAT_t             *myFAT;
struct file_descriptor_t fd_table[FS_OPEN_MAX_COUNT]; 


/* Makes the file system contained in the specified virtual disk "ready to be used" */
int fs_mount(const char *diskname) {

	mySuperblock = malloc(sizeof(struct superblock_t));

	// open disk 
	if(block_disk_open(diskname) < 0){
		fprintf(stderr, "Error - Failure to open disk - fs_mount.\n");
		return -1;
	}
	
	// initialize data onto local super block 
	if(block_read(0, mySuperblock) < 0){
		fprintf(stderr, "Error0: failure to read from block - fs_mount.\n");
		return -1;
	}
	//printf("Signature: %s\n", mySuperblock->signature);

	if(strncmp(mySuperblock->signature, "ECS150FS",8 ) !=0){
		fprintf(stderr, "Error1: incorrect signature -fs_mount.\n");
		return -1;
	}

	if(mySuperblock->numBlocks != block_disk_count()) {
		fprintf(stderr, "Error2: incorrect block disk count - fs_mount.\n");
		return -1;
	}

	/* FAT Creation */
	int FAT_blocks = ((mySuperblock->numDataBlocks) * 2)/BLOCK_SIZE; // the size of the FAT (in terms of blocks)
	if(FAT_blocks == 0)
		FAT_blocks =1;

	myFAT = malloc(sizeof(struct FAT_t) * FAT_blocks);
	for(int i = 1; i <= FAT_blocks; i++) {
		// read each fat block in the disk starting at position 1
		if(block_read(i, myFAT + (i * BLOCK_SIZE)) < 0) {
			fprintf(stderr, "Error3: failure to read from block - fs_mount.\n");
			return -1;
		}
	}

	/* Root Directory Creation */
	myRootDir = malloc(sizeof(struct rootdirectory_t) * FS_FILE_MAX_COUNT);
	if(block_read(FAT_blocks + 1, myRootDir) < 0) { // FAT_blocks is size of fat - Root Directory starts here.
		fprintf(stderr, "Error: 4 - failure to read from block - fs_mount.\n");
		return -1;
	}
	
	return 0;
}


/* Makes sure that the virtual disk is properly closed and that all the internal data structures of the FS layer are properly cleaned. */
int fs_umount(void) {

	if(!mySuperblock){
		fprintf(stderr, "Error: No disk available to unmount - fs_unmount.\n");
		return -1;
	}

	free(mySuperblock->signature);
	free(mySuperblock);
	free(myRootDir->filename);
	free(myRootDir);
	free(myFAT);
	block_disk_close();
	return 0;
}

/* 
 * to create a test file system call ./fs.x make test.fs 8192 to 
 * make a virtual disk with 8192 datablocks) 
 */


/* Display some information about the currently mounted file system. */
int fs_info(void) {

	int FAT_blocks = ((mySuperblock->numDataBlocks) * 2)/BLOCK_SIZE; // the size of the FAT (in terms of blocks)
	if(FAT_blocks == 0)
		FAT_blocks =1;

	int i, count;
	for(i = 0; i < 127; i++) {
		printf("Count: %d ,%04x\n",count, (myRootDir+(i * 31))->filename[0]);
		if((myRootDir+(i * 31))->filename[0] == 0x00)
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
	/* PART 3 - Phase 1 */
}


/*
Add File:
	0. If it is the first time around, the name needs to be set, and all other information gets reset.
	1. Find an empty entry in the root directory.
	2. Add information

*/
int fs_create(const char *filename) {
	char loc = locate_file(filename);

	
	return 0;
	/* TODO: PART 3 - Phase 2 */
}

/*
Remove File:
	1. Empty file entry and all its datablocks associated with file contents from FAT.

*/
int fs_delete(const char *filename) {
	
	return 0;
	/* TODO: PART 3 - Phase 2 */
}

/*
which prints the listing of all the files in the file system. Make sure that the output corresponds exactly to the reference program.
*/
int fs_ls(void) {
	
	return 0;
	/* TODO: PART 3 - Phase 2 */
}

int fs_open(const char *filename) {
	
	return 0;
	/* TODO: PART 3 - Phase 3 */
}

int fs_close(int fd) {

	return 0;
	/* TODO: PART 3 - Phase 3 */
}

int fs_stat(int fd) {

	return 0;
	/* TODO: PART 3 - Phase 3 */
}

int fs_lseek(int fd, size_t offset) {
	
	return 0;
	/* TODO: PART 3 - Phase 3 */
}

int fs_write(int fd, void *buf, size_t count) {

	return 0;
	/* TODO: PART 3 - Phase 4 */
}

int fs_read(int fd, void *buf, size_t count) {

	return 0;
	/* TODO: PART 3 - Phase 4 */
}


// returns the file index where the provided file
// is located in the root directory 
char locate_file(const char* file_name)
{
    char i;
    for(i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if(strcmp(myRootDir[i].filename, file_name) == 0) {
            return i;  
        }
    }

    // file not found
    return -1;      
}