# File System - ECS150 Winter, 2017

### Table of Contents
1. Introduction & Logical Components
2. Phase 1: Mounting/Un-mounting
3. Phase 2: File Creation/Deletion
4. Phase 3: File Descriptor Operations
5. Phase 4: File Reading/Writing
6. Limitations and Possible Improvements


_______________________________________________________________________________

## Introduction
The goal of this project is to extend our user-level thread library with the support of a very simple file system, ECS150-FS. This file system is based on a FAT and supports up to 128 files in a single root directory.

Exactly like real hard drives which are split into sectors, the virtual disk is logically split into blocks. The first software layer involved in the file system implementation is the block API which is used to open or close a virtual disk, and read or write entire blocks from it. Through the File System layer, you can mount a virtual disk, list the files that are part of the disk, add or delete new files, read from files or write to files.

The layout of the File System on a disk is composed of four consecutive logical parts and are listed below in order of their location on the disk:

* **_Superblock_** - The superblock is the very first block of the disk and contains the essential information about the file system such as the signature of the disk, number of blocks it contains, the index of the root directory on the disk, the index of the data blocks on the disk, and the amount of FAT and Data blocks. 

* **_File Allocation Table_** - the FAT is located on one or more blocks, and keeps track of both the free data blocks and the mapping between files and the data blocks holding their content. This block follows the the superblock on the disk and is represented as an array of structs containing a 16 bit variable.

* **_Root Directory_** - The root directory is one data block which follows the FAT blocks and contains an entry for each file of the file system. It is represented as a struct and defines the files name, size and the location of the first data block for this file. Since the maximum number of files in a directory is 128 files, the root directory is an array of 128 of these structs.

* **_Data Blocks_** - Finally, all the remaining blocks are Data Blocks and are used by the content of files. Since the size of each virtual disk block is 4096 bytes, a file may span several blocks depending on its size. If a file is smaller than the size of a block, it will still occupy that specific block regardless of the extra space on that block. This causes internal fragmentation within our virtual disk and should be kept in mind.

_______________________________________________________________________________


### Phase 1: Mounting/Un-mounting

#### Implementation Details:

This phase makes the file system contained in the specified virtual disk "ready to be used". This means that all the logical components must be 'initialized', or copied from the virtual disk with the proper information. In order to do this, the Block API is used to read each needed block into our memory. For each logical component, the program `malloc`s the needed space per component which gives the right amount of space when doing the block reads. In the case of the Superblock, although the variables which contain the essential information of the disk do not occupy an equivalent size of memory as a block on the virtual disk, the struct contains an extra 'padding' which takes up the extra space which is not used and gives the struct the correct size, 4096 bytes. This same strategy is used for the root directory struct as well.

The FAT struct has a similar approach to reading its blocks into memory, but the logic for doing so is slightly tweaked in order to accomodate the varied size of the FAT. Since we read in the Superblock before the FAT, we are able to know the size of the FAT blocks. To `malloc` the right amount of space on our memory, we make sure that we allocate the right amount of size for the *blocks* the fat occupies, rather than the actual size of the memory within the FAT blocks. This is done by setting a global pointer to the FAT struct with the call to malloc: `malloc(mySuperblock->numFAT * BLOCK_SIZE)`. This allocates the space corresponding to the size of the FAT blocks. We are now able to increment by the number of blocks, and read in the information into the space we put in memory. Each iteration increments the position in our memory by the size of a block.

#### API Details

##### fs_mount:  

* Initializes space in memory for the corresponding logical components; the Superblock, FAT, and Root Directory. 

* Using the _Block API_: `block_read()` takes in two parameters; the block index, and the memory in which you want to read to. This function allows the program to copy the memory in the blocks into the memory that we allocated space for. We are then able to use this memory to modify and execute other operations. 

* It's important to notice that in doing these operation on the memory (and not on the actual memory on the disk), it does not get updated until the progam calls `fs_unmount`, which is described below.

##### fs_umount

* After executing various commands in memory which we allocated, the program must now write all these modification back to the virtual disk. 

* Using the _BLOCK API_: `block_write()` takes in two parameter; the block index, and the memory which you want to read from. This functino allows the program to read the modified memory back into the specified blocks. By doing this, the virtual disk gets updated with all the meta-information and file data (the operations on the file system itself).

* After writing to disk, the program is required to free the memory in our logical components in order for other operation to use these structures without any unwanted memory. This is done by calling `free()` on all three components: `mySuperblock`, `myRootDir`, `myFAT`.


##### fs_info

* After succefully mounting and unmounting a file system, the program is able to print some information about the mounted file system. The information is all located in the Superblock struct. Calling this function displays information about the total number of blocks, FAT blocks, how many free FAT blocks there are, and how many files are free to be used (since the limit is 128).

* By maintaining this information in the superblock component, the program is able to utilize this information not only in the `fs_info` function, but through out other important commands.

_______________________________________________________________________________


### Phase 2: File Creation/Deletion

This phase enables the user to add or remove files from the file system, however, it does not yet support the ability to write the contents of the file which you are adding into the disk. In order to add a file, the program finds  an empty entry in the root directory. The program identifies an open entry by checking the first byte of the entry. If this first byte is zero (or EMPTY, a macro for 0) then the program initializes the entry's filename with the passed in arguement, the file size to zero, and the data block index to _end-of-chain_ (or EOC, a macro for the value 0xFFFF).

The procedure is opposite to remove a file. The program ensure that the file's entry is emptied and all of the data blocks containing the file's contents are freed in the FAT. The program handles this by first locating the file in the root directory using t helper function `locate_file()`. This enables the program to have the necessary information, specifically the location of the file within the data blocks, to remove and reset the information.


#### API Details

##### fs_create

* Locates the first available entry in the root directory array and initializes its file name, its size to zero and its data block index to EOC

* It's important to note that this phase does not handle 'writing' the contents of the file into the virtual disk. It only initializes the file to be ready for writing or reading. 

##### fs_delete

* Locates the file index via the helper function `locate_file()`, which returns the beginning index of the file within the virtual disk's Data Block section.

* Resets the corresponding FAT array elements in which the file is located in to 0, which indicates that the space is free to use. Then rests the the filename in the root directory entry to zero, and the file size to zero.

##### fs_ls

* Simply locates the entries that are occupied, and prints a list of the files that are currently on disk. Because the program successfuly created the files, this function can recognize the occupied spaces in the root directory array with the valid information. 

_______________________________________________________________________________

### Phase 3: File Descriptor Operations

Phase 3 introduces fucntion which are very similar to the Linux file system operations to the FS API. In order for applications to manipulate files fs_open() opens a file and returns a file descriptor which can then be used for subsequent operations on this file (reading, writing, changing the file offset, etc). fs_close() closes a file descriptor.

Your library must support a maximum of 32 file descriptors that can be open simultaneously. The same file (i.e. file with the same name) can be opened multiple times, in which case fs_open() must return multiple independent file descriptors.

A file descriptor is associated to a file and also contains a file offset. The file offset indicates the current reading/writing position in the file. It is implicitly incremented whenever a read or write is performed, or can be explicitly set by fs_lseek().

Finally, the function fs_stat() must return the size of the file corresponding to the specified file descriptor. To append to a file, one can, for example, call fs_lseek(fd, fs_stat(fd));.


#### API Details

##### fs_open

* Description ...

##### fs_close

* Description ...

##### fs_stat

* Description ...

##### fs_lseek

* Description ...

_______________________________________________________________________________

### Phase 4: File Reading/Writing

Description...

#### API Details

##### fs_write

* Description ...

##### fs_read

* Description ...

_______________________________________________________________________________

### Limitations and Possible Improvements

Description...




