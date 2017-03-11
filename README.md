# File System - ECS150 Winter, 2017

### Table of Contents
1. Introduction & Logical Components
2. Phase 1: Mounting/Un-mounting
3. Phase 2: File Creation/Deletion
4. Phase 3: File Descriptor Operations
5. Phase 4: File Reading/Writing
6. DataStructures Justifications
7. Limitations and Possible Improvements	


_______________________________________________________________________________

## Introduction
The goal of this project is to extend our user-level thread library with the support of a very simple file system, ECS150-FS. This file system is based on a FAT and supports up to 128 files in a single root directory.

Similiar to how real hard drives are split into sectors, the virtual disk is logically split into blocks. The first software layer involved in the file system implementation is the block API which is used to open, close, read, and write to or from at block level. With the abstraction of the File System layer, it comes possible to mount a virtual disk, and maintain a persistent form of storage just from a consecutive string of bytes just like any real disk. 

The layout of the File System on a disk is composed of four consecutive logical parts and are listed below in order of their location on the disk:

![Alt text](/read_me_files/FAT_layout.png "FAT Layout")

* **_Superblock_** - The superblock is the very first block of the disk and contains the essential meta-data about the file system such as the signature of the disk, number of blocks it contains, the index of the root directory on the disk, the index of the data blocks on the disk, and the amount of FAT and Data blocks. 

* **_File Allocation Table_** - the FAT is located on one or more blocks, and keeps track of both the free data blocks and the mapping between files and the data blocks holding their content. This block follows the the superblock on the disk and is represented as an array of structs containing a 16 bit variable. The FAT number of entries itself is proprortional to the number of datablocks 1:1.

* **_Root Directory_** - The root directory is one data block which follows the FAT blocks and contains an entry for each file of the file system. It is represented as a struct and defines the files name, size and the location of the first data block for this file. Since the maximum number of files in a directory is 128 files, the root directory is an array of 128 of these structs.

* **_Data Blocks_** - Finally, all the remaining blocks are Data Blocks and are used by the content of files. Since the size of each virtual disk block is 4096 bytes, a file may span several blocks depending on its size as indicated by its offset in the file descriptor table. If a file is smaller than the size of a block, it will still occupy that specific block regardless of the extra space on that block. This causes internal fragmentation within our virtual disk and should be kept in mind.

#### Usage

```
./run.sh
```

_______________________________________________________________________________


### Phase 1: Mounting/Un-mounting

#### Implementation Details:

This phase makes the file system contained in the specified virtual disk "ready to be used". This means that all the logical components must be 'initialized', or copied from the virtual disk with the proper information. In order to do this, the Block API is used to read each needed block into our memory. For each logical component, the program `malloc`s the needed space per component which gives the right amount of space when doing the block reads. In the case of the Superblock, although the variables which contain the essential information of the disk do not occupy an equivalent size of memory as a block on the virtual disk, the struct contains an extra 'padding' which takes up the extra space which is not used and gives the struct the correct size, 4096 bytes. This same strategy is used for the root directory struct as well.

The FAT struct has a similar approach to reading its blocks into memory, but the logic for doing so is slightly tweaked in order to accomodate the varied size of the FAT. Since we read in the Superblock before the FAT, the program is able to know the size of the FAT blocks. To `malloc` the right amount of space on memory, the program allocates the right amount of space for the *blocks* that the FAT occupies, rather than the actual size of the memory within the FAT blocks. This is done by setting a global pointer to the FAT struct with the call to malloc: `malloc(mySuperblock->numFAT * BLOCK_SIZE)`. This allocates the space corresponding to the size of the FAT blocks. Each iteration increments the position in our memory by the size of a block.

#### API Details

##### fs_mount:  

* Initializes meta data in memory for the corresponding logical components; the Superblock, FAT, and Root Directory. 

* Using the _Block API_: `block_read()` takes in two parameters; the block index, and the memory in which you want to read to. This function allows the program to copy the memory in the blocks into the memory that was allocated on RAM. At this point, the program is able to use this memory to modify and execute other operations. 

* It's important to notice that in doing these operation on RAM memory (and not on the actual memory on the virtual disk), the virtual disk does not get updated until the progam calls `fs_unmount`, which is described below.

##### fs_umount

* After executing various commands in the memory allocated, the program must now write all these modification back to the virtual disk. It must also ensure that all data structures are cleaned and properly closed. Additionally, just like in `fs_mount`, it verifies that the file system has the expected format, and accomodates for any failures that get written out to disk.

* Using the _BLOCK API_: `block_write()` takes in two parameter; the block index, and the memory which you want to read from. This function allows the program to read the modified memory back into the specified blocks. By doing this, the virtual disk gets updated with all the meta-information and file data (the operations on the file system itself).

* After writing to disk, the program is required to free the memory in the logical components in order for other operations to use these structures without any unwanted memory. This is done by calling `free()` on all three components: `mySuperblock`, `myRootDir`, `myFAT`.

* Although it was not absolutely nessesary to reset the file descriptors, just for assurance purposes the program ensures to mark them all as unused.

##### fs_info

* After succefully mounting and unmounting a file system, the program is able to print some information about the mounted file system. This information is all located in the Superblock struct. Calling this function displays information about the total number of blocks, FAT blocks, how many free FAT blocks there are, and how many files are free to be used (as the limit is 128). This information could be considered as the psuedo - fs layer of the file system, since its contents are generated dynamically, and can certainly change at any given moment.

* For the most part, doing this just means reading the information that is currently held by the global superblock. However, for information such as `fat_free_ratio` or `rdir_free_ratio`, we modularized seperate functions that will linearly search through all the directory entries, or all the FAT entries and count the information.

_______________________________________________________________________________


### Phase 2: File Creation/Deletion

This phase enables the user to add or remove files from the file system, however, it does not yet support the ability to write the contents of the file which you are adding into the disk. In order to add a file, the program finds  an empty entry in the root directory. The program identifies an open entry by checking the first byte of the entry. If this first byte is zero (or EMPTY, a macro for 0) then the program initializes the entry's filename with the passed in arguement, the file size to zero, and the data block index to _end-of-chain_ (or EOC, a macro for the value 0xFFFF).

The procedure is opposite to remove a file. The program ensure that the file's entry is emptied and all of the data blocks containing the file's contents are freed in the FAT. The program handles this by first locating the file in the root directory using t helper function `locate_file()`. This enables the program to have the necessary information, specifically the location of the file within the data blocks, to remove and reset the information.


#### API Details

##### fs_create
* The function begins by doing error checking such as checking if a user is creating a file that already exists, if its allocation conflicts with the number of possible directory entries, or if the name of the file exceeds the limited amount.

* Then it locates the first available entry in the root directory array and initializes its file name, its size to zero and its data block index to EOC

* It's important to note that this phase does not handle 'writing' the contents of the file into the virtual disk. It only initializes the file to be ready for writing or reading. 

##### fs_delete
* A file is open if its appropriate file descriptor is in use. We ensure that when a file is in use, it cannot be removed. To do this, begin by first locating the file index in the root directory. Then it becomes just a matter of iterating through the file descriptors and matching the file names and checking that it is use.

* The file index via the helper function `locate_file()`, which returns the beginning index of the file within the virtual disk's Data Block section.

* Resets the corresponding FAT array elements in which the file is located in to 0, which indicates that the space is free to use. This process is just a matter of beginning in the first data block as indicated by the root directory, and then jumping throughout the FAT table marking files as EMPTY, until ofcourse the EOC is reached.

* Finally, the filename and cooresponding size in the root directory entry resets back to zero.

##### fs_ls

* Simply locates the entries that are occupied, and prints a list of the files that are currently on disk. Because the program successfuly created the files, this function can recognize the occupied spaces in the root directory array with the valid information. 

* The program ensures that the information read on root directory accounts for the management of files in real time.

_______________________________________________________________________________

### Phase 3: File Descriptor Operations

The file descriptor table was important for the management of both opening and closing files. When managed correctly, an FD can be used for subsequent operations such as reading or writing to the file it cooresponds to (by referencing the files offset). This library supports a maximum of 32 file descriptors.


#### API Details

##### fs_open

* The function begins by doing error checking. It checks that the file itself must exist, and that allocating a new file descriptor cannot exceed the limit of file descriptors.

* Then its just a matter of `locate_avail_fd` which returns the index for the first available file descriptor. This supports files that can be opened multiple times (with independent fds) so long as it doesnt exceed information in the bullet point above. Using a boolean variable, the function tracks when a file descriptor is `in_use` or not.

##### fs_close

* The recipe for `fs_close` was directly a mirror opposite of `fs_open`. Since it is continously referencing `is_used` consistently when referencing a file descriptor, it just had to set it back to false.

##### fs_stat

* Return the size of the file corresponding to the specified file descriptor. 

* This was a matter of locating the filename from the file descriptor table, identifing the correct root directory, and finally identifying the filesize.

##### fs_lseek

* Explicitly sets a cooresponding file descriptors offset. This essentially moves the current head of the reading or writing position.
This function ensures that the new offset doesn't overload the size of the file (otherwise it would be going out of bounds of the file and that wouldnt make sense). With the help of `fs_stat` it obtains the size of the file and then perform a simple check that the provided offset doesnt exceed this value.

_______________________________________________________________________________

### Phase 4: File Reading/Writing

This final phase implements the functionality for reading and writing to/from a file. Because the complexity of these functions are quite big, many of the functions are modularized into static helper functions. For example, one of the functions returns the index of the data block corresponding to the file's offset. Both read and write relate with one another fairly well so, alot of the ideas between them also go hand in hand. It is important to note that the allocation of new blocks follow the first-fit strategy (first block available from the beginning of the FAT).

When reading in a file, the function first begins by initizliaing a few variables with essential information. These variables include the file name and offset refernced by the file descriptor arguenemt, the file index which gets located by calling a static helper function called `locate_file()`, and lastly, creating a root directory pointer to the location in the root directory array. 

#### API Details

##### fs_read
* Begins by initizliaing a few variables with essential information. These variables include the file name and offset refernced by the file descriptor arguenemt, the file index which gets located by calling a static helper function called `locate_file()`, and lastly, creating a root directory pointer to the location in the root directory array.  

* The algorithm identifies the amount of bytes that is requested to be read. Even though this is provided, there are still some complications. For example, if the offset of the file and the amount requested to be read exceeds the file size, then the it couldn't make sense to read past the filesize of the file. In this case, the algorithm will read the most it can: `abs(the_dir->file_size - offset)`. Otherwise, it is just the `count` of number of bytes initially requested.

* Next is identifying the number of blocks to be read. `(amount_to_read / BLOCK_SIZE) + 1` identifies this by calculating how many blocks can fit into the requested number of bytes to be read.

* Next is identifying the starting block that is requested from the current offset of the file. For this, `offset / BLOCK_SIZE` returns how many blocks we have to iterate through the FAT table before we can begin doing a read on byte level.

* Next we identifying on which byte we have to begin in the current block of the current offset. `offset % BLOCK_SIZE` returns the remainder, which is essentially the offset position of the block.

* By Using what's known as a bounce buffer, we are finally able to begin doing our read operations. The bounce buffer is a key component in reading the files. It allows for reading in the files content block by block. This is done by iterating through a for loop with the number of blocks we identified to read. 

* We also introduce a new variable, `left_shift`, which accounts for the total amount of bytes that were read into the bounce buffer at a given moment. For example, if we were reading a very large file in the beginning, then the actual amount we are reading is actually a `BLOCK_SIZE` worth, subtracted by the offset. When the reading finally reaches its conclusion, `left_shift` simply becomes the remainder of the final bits of the block.
    * This was nessessary for a number of reasons. For example, the offset of a file could be completely arbitarary, we just had to account for the offset of the file in the very beginning.
    * From that point forward, however, we set `location = 0`, which says that the next block is immediately located from the next FAT index, so the next bounce buff becomes completely filled.

* As stated above, this process reads a `BLOCK_SIZE` into the bounce buffer one at a time. We make sure to properly offset the position in the `bounce_buff` in order to account for any sort of offset (see diagram below). Within every iteration, we locate the next block to read with `FAT_iter = FAT_blocks[FAT_iter].words;`.

```
		//0000000000000000000000000000000000DataDataDataData
		// 								^ bouce_buff + loc 
```

* Finally we perform a memcpy to add to the buffer in the arguments. In the next iteration, the read buffer location increments to the amount that was actually read (as identified by `left_shift`). We also use this same strategy with `write`, since it ensure that we `memcpy` as much as we possibly can before the file contents overwhelms the disk size.

* As identified, the special cases of writing or reading to and/or from a file happen mostly for small reading/writing operations, or at the beginning or end of a big operation. 

##### fs_write

* Similar to `fs_read`, this function begins by initializing a few important vairbales found by the file descriptor. 

##### There are a few cases to notice when preforming a write: 

1. _Writing to an empty file_: If writing to a new file, the offset will be 0, the filesize will be 0, and the root directory entry will be EOC. This is the simple case, and gets handled by calculating the number of blocks that are needed to write the entire count of the buffer. A bounce buffer is then initialized, and an iteration to find the available FAT blocks gets run. `memcpy()` handles the copying of the contents per block basis, and writes to the virtual disk. 

2. _Writing a larger file than the disk_:Another edge case is when a user is trying to write a file which exceeds the amount of data blocks currently available on disk. In this case, we check against the total amount of blocks needed to be written and the amount of free FAT blocks available. By doing this, the program makes sure that only the correct amount that can fit on the disk gets written. 

3. _Writing with a given offset_: This gets handled by over writing the contents in the given data block at the specified offset and discarding the rest by ending the copied buffer contents with a null character. 

In general, there were many other special cases which we took care of. Reading the `fs_write` code will clarify all the special cases that were covered. 



_______________________________________________________________________________

### Data Structures and Other Design Justifications
* We decided to keep all of our structs global, since nearly all the functions in `fs.c` communicate with them. This also simplifies the process in `fs_unmount` at the end, since it can write the changes we've acknowledged on our global structs back onto the virtual disk.

**File Descriptor Table**
* In addition to the `is_used`, `offset`, and `file_index` we included `file_name`. This wasn't entirely nessessary, but since we realized we were using it frequently throughout our program, including it reduced a few layers of code.


_______________________________________________________________________________

### Limitations and Possible Improvements

Error checking on its own contributed to the most line of code. Possibly modularizing all this information into a seperate error handling structures or into an entirely new class would greatly increase the readability of our code.

The writing segment of this program does not handle writing into a file with an offset correctly. As opposed to inserting the content to be written into the offset location and shifting the contents past the offset, this function overwrites the contents past the offset. To imporve this, we would have calculated the extra blocks that would be needed, search for the available blocks in the FAT array, then write the data of the buffer to the blocks, and finally change the contents of the FAT array with the newly added blocks.

As mentioned before, this implementation is very close to the original FAT32 File System, but not quite as complex. It is important to be aware of its limitations and advantages.



Pros:
	* Simple
	* State required per file: start block only
	* Widely supported
	* No external fragmentation
	* Relatively fast if FAT is cached in RAM

Cons:
	* Poor locality
	* Many seeks if FAT is on disk
	* Poor random access
	* Limited metadata
	* Limited access control
	* No support for hard links
	* Limitations on volume and file size
	* No support for reliability strategies






