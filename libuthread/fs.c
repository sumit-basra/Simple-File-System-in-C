#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _UTHREAD_PRIVATE
#include "disk.h"
#include "fs.h"


/*
 * Describing the Data Structures correctly using stdint.h -> int8_t, uint8_t, uint16_t
 *
 *
 * Superblock:
 * The superblock is the first block of the file system. Its internal format is:
 *
 * Offset   Length (bytes)  Description
 * 0x00     8-              Signature (must be equal to "ECS150FS")
 * 0x08     2-              Total amount of blocks of virtual disk
 * 0x0A     2-              Root directory block index
 * 0x0C     2-              Data block start index
 * 0x0E     2               Amount of data blocks
 * 0x10     1               Number of blocks for FAT
 * 0x11     4079            Unused/Padding
 *
 * FAT:
 * The FAT is a flat array, possibly spanning several blocks, which entries are composed of 16-bit unsigned words.
 * There are as many entries as data *blocks in the disk.
 *
 *
 *
 *
 */
typedef struct __attribute__((packed)) {
    int64_t signature;
    int16_t numBlocks;
    int16_t rootDirInd;
    int16_t dataInd;
    int16_t numDataBlocks;
    int8_t  numFAT;
    // Unused/Padding ?
 
} superblock_t;
 
struct superblock_t *mySuperblock;
 
 
int fs_mount(const char *diskname)
{
    // allocate superblock
    mySuperblock = malloc(sizeof(struct superblock_t));
	block_disk_open(diskname);
	if(block_read(0, mySuperblock)){
		fprintf(stderr, "ERROR: Failure to read disk blocks.\n");
		return -1;
	}
    /* TODO: PART 3 - Phase 1 */
}




int fs_mount(const char *diskname)
{
	/* TODO: PART 3 - Phase 1 */
}

int fs_umount(void)
{
	/* TODO: PART 3 - Phase 1 */
}

int fs_info(void)
{
	/* TODO: PART 3 - Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: PART 3 - Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: PART 3 - Phase 2 */
}

int fs_ls(void)
{
	/* TODO: PART 3 - Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: PART 3 - Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: PART 3 - Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: PART 3 - Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: PART 3 - Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: PART 3 - Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: PART 3 - Phase 4 */
}

