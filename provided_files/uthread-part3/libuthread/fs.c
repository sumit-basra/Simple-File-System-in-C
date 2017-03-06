#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _UTHREAD_PRIVATE
#include "disk.h"
#include "fs.h"

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

