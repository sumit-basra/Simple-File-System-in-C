#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "uthread.h"
#include "fs.h"

#define test_fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define die(...)			\
do {					\
	test_fs_error(__VA_ARGS__);	\
	exit(1);			\
} while (0)

int curr_fd;

int info(char* diskname) 
{
	/* display information and unmount */
	fs_info();
	return 0;
}

int ls(char* diskname)
{
	fs_ls();
	return 0;
}

int add(char* filename, int offset)
{
	int fd, fs_fd;
	struct stat st;
	char *buf;
	size_t written;

	/* Open actual file on computer */
	fd = open(filename, O_RDONLY);

	/* Check if file opened correctly */
	if (fd < 0)
		die("open failed");
	if (fstat(fd, &st))
		die("fstat failed");
	
	/* create buffer for file */
	buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(!buf)
		die("mmap failed");

	/* write to file */
	written = fs_write(curr_fd, buf, st.st_size);

	printf("Wrote file '%s' (%zd/%zd bytes)\n", filename, written, st.st_size);

	munmap(buf, st.st_size);
	close(fd);

	return 0;
}

int cat(char* filename, int offset)
{
	char *buf;
	int fd;
	size_t stat, read;
	
	stat = fs_stat(curr_fd);
	if (stat < 0) {
		curr_fd = -1;
		fs_umount();
		die("Cannot stat file");
	}
	if (!stat) {
		/* Nothing to read, file is empty */
		printf("Empty file\n");
		return 0;
	}

	buf = malloc(stat);
	if(!buf) {
		perror("malloc");
		fs_umount();
		die("Cannot malloc");
	}

	read = fs_read(curr_fd, buf, stat);

	printf("Read file '%s' (%zd/%zd bytes)\n", filename, read, stat);
	printf("Content of the file:\n%s", buf);

	free(buf);

	return 0;
}

int rm(char* filename)
{
	if (fs_delete(filename)) {
		fs_umount();
		die("Cannot delete file");
	}

	printf("Removed file '%s'\n", filename);

	return 0;
}

int my_open(char *filename)
{
	curr_fd = fs_open(filename);
	if (curr_fd < 0) {
		curr_fd = -1;
		fs_umount();
		die("Cannot open file");
	}

	return 0;
}

int my_close(char* filename)
{
	if (fs_close(curr_fd)) {
		curr_fd = -1;
		fs_umount();
		die("Cannot close file");
	}

	curr_fd = -1;
	return 0;
}

int create(char* filename)
{
	if ((curr_fd = fs_create(filename))) {
		curr_fd = -1;
		fs_umount();
		die("Cannot create file");
	}

	return 0;
}

int operation(char* command, char* diskname, char* filename, int offset) 
{
	if (strcmp(command, "info") == 0) {
		/* display info */

		return(info(diskname));		
	} else if (strcmp(command, "ls") == 0) {
		/* display list of files */

		return(ls(diskname));
	} else if (strcmp(command, "add") == 0) {
		/* add file to virtual disk */

		return(add(filename, offset));
	} else if (strcmp(command, "cat") == 0) {
		/* print file */

		return(cat(filename, offset));
	} else if (strcmp(command, "rm") == 0) {
		/* remove file */

		return(rm(filename));
	}  else if (strcmp(command, "mount") == 0) {
		/* mount virtual disk */

		if(fs_mount(diskname))
			die("Cannot mount disk");
	} else if (strcmp(command, "umount") == 0) {
		/* unmount virtual disk */

		if(fs_umount())
			die("Cannot unmount disk");
	} else if (strcmp(command, "open") == 0) {
		/* open file on virtual disk */

		return my_open(filename);
	} else if (strcmp(command, "close") == 0) {
		/* close file on virtual disk */

		return my_close(filename);
	} else if (strcmp(command, "create") == 0) {
		/* create file */

		return(create(filename));
	} else if (strcmp(command, "switch") == 0) {
		curr_fd = offset;
	} else if (strcmp(command, "lseek") == 0) {
		if(fs_lseek(curr_fd, offset))
			die("lseek died");
	}	
	else {
		/* no matching command: error */
		return -1;
	}

	return 0;
}


int main() 
{
	char buffer[256];
	char command[32];
	char diskname[32];
	char filename[32];
	int offset;

	curr_fd = -1;

	/* constantly run filesystem commands in loop */
	printf("$");
	while(fgets(buffer, 256, stdin) != NULL) {
		sscanf(buffer, "%s %s %s %d", command, diskname, filename, &offset);
		printf("$");
		if(operation(command, diskname, filename, offset))
			return -1;
	}
	
	return 0;
}
