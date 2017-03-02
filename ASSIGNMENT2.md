% ECS 150: Project #3 - User-level thread library
%
% UC Davis, Winter Quarter 2017

# General information

Due before **11:59 PM, Friday, Feb 24th, 2017**.

You will be working with a partner for this project.

The reference work environment is the CSIF.

# Specifications

*Note that the specifications for this project are subject to change at anytime
for additional clarification.*

## Introduction

The goal of this project is to extend your user-level thread library with
per-thread protected memory regions. Such regions, which we call
pseudo-**Thread-Local Storage** (TLS), aim to provide private heap to each
thread that requires it.

Exactly like with OS kernels, this new memory management will be implemented in
layers. The first layer will be global to the library and will distribute memory
pages: it's the page allocator. Above the page allocator, the TLS layer will be
specific to each thread and will offer a `malloc()/free()`-like interface to
allocate bytes of memory. Each TLS area will be protected when the thread it
belongs to is not running.

A working example of the thread library can be found on the CSIF, at
`/home/jporquet/ecs150/libuthread.a`.

### Skeleton code

The skeleton code that you are expected to complete is available in the archive
`/home/jporquet/ecs150/uthread-part2.zip`. This code already defines most of the
prototypes for the functions you must implement, as explained in the following
sections.

```
$ tree
.
├── libuthread
│   ├── bitmap.c*
│   ├── bitmap.h
│   ├── context.c
│   ├── context.h
│   ├── Makefile*
│   ├── palloc.c*
│   ├── palloc.h
│   ├── preempt.c
│   ├── preempt.h
│   ├── queue.c
│   ├── queue.h
│   ├── semaphore.c
│   ├── semaphore.h
│   ├── tls.c*
│   ├── tls.h
│   ├── uthread.c*
│   └── uthread.h
├── Makefile
└── test-tls.c
```

As you can observe, the organization hasn't changed from the previous project.
Some new files have appears in the subdirectory `libuthread`, that you must
complete. The files to complete are marked with a star (you normally should not
have to touch any of the files which are not marked with a star).

Compare with your existing project so that you only import the files that you
don't already have.

## Phase 1: bitmap API

In this first phase, you must implement a bitmap. The interface to this queue is
defined in `libuthread/bitmap.h` and your code should be added into
`libuthread/bitmap.c`.

When defining the data structure for your bitmap, keep in mind that the main
advantage of a bitmap is to save memory consumption by representing the
availability of resources with bits.

### 1.1 Testing

Add a new test program in the root directory (e.g. `test-bitmap.c`) which tests
your bitmap implementation. It is important that your test program is
comprehensive in order to ensure that your bitmap implementation is as resistant
as possible. It will ensure that you don't encounter bugs when using your bitmap
later on.

## Phase 2: palloc API

In this second phase, you must implement the memory page allocator. The
interface to this palloc API is defined in `libuthread/palloc.h` and your code
should be added into `libuthread/palloc.c`.

At the beginning of an application and if TLS is required for this application,
the main function calls `uthread_mem_config()` before starting the threading
management, and indicates what should be the total number of memory pages
available in the system.

When initializing the threading system in `uthread_start()`, the palloc layer
should be created and the number of specified memory pages allocated. The
allocation should be performed with the C library function `mmap()`.

Once the pool of pages is allocated, the availability of the pages should be
represented by a bitmap. Pages are allocated and freed through the functions
`palloc_get_pages()` and `palloc_free_pages()` that you must implement.

## Phase 3: TLS API

The goal of the TLS layer is to provide a private and protected heap to each
thread that requires it.

The interface of the TLS API is defined in `libuthread/tls.h` and your
implementation should go in `libuthread/tls.c`. The API is split into a public
API (for the applications to use) and a private API (for the libuthread to use
internally).

### Public API

A thread that requires a TLS area which call `tls_create()` and specify the
number of memory pages that it needs. For your information, a memory page for
this project is worth 4,096 bytes.

Once the TLS area is created, using pages received by the page allocator that
you have implemented, a thread is able to allocate and free dynamic bytes of
memory through the functions `tls_alloc()` and `tls_free()`.

The algorithm for the TLS memory allocation should be *first-fit*. The TLS
allocator keeps a list of free memory blocks and, upon receiving a request for
memory allocation, scans along this list for the *first* block that is large
enough to satisfy the request.

Note that it is not recommended to use your queue API for the TLS allocator,
since blocks will not be enqueued and dequeued with a FIFO strategy.

Upon memory deallocation, the used block of memory must be reinserted in the
free list and possibly merged with adjacent free blocks in order to maximize the
available free memory.

### Private API

The TLS area is to be kept private for each thread. In order to enforce that a
thread cannot have access another thread's TLS, the TLS belonging to a thread
will be protected each time this thread is unscheduled, and unprotected only
when the it runs.

The TLS API defines two functions, `tls_open()` and `tls_close()`, which
respectively give access or close access to the TLS area of the currently
running thread. Such operations should internally be performed with the C
library call `mprotect()`.

Finally, the TLS area of each thread shall be kept in their TCB. So in order to
set or get the TLS pointer, you must modify `libuthread/uthread.[ch]` and
implement the functions `uthread_set_tls()` and `uthread_get_tls()`.

### Testing

Only one testing programs is available in order to test your TLS
implementation:

- `test-tls`: a simple test which verifies the TLS allocation algorithm

You should therefore write more tests and explain their relevancy in your
report file.

# Deliverable

## Constraints

Your library must be written in C, be compiled with GCC and only use the
standard functions provided by the GNU C Library. It cannot be linked to any
other external libraries.

Your source code should follow the relevant parts of the [Linux kernel coding
style](https://www.kernel.org/doc/html/latest/process/coding-style.html) and be
properly commented.

## Content

Your submission should contain, besides your source code, the following files:

- `AUTHORS`: full name, student ID and email of each partner, one entry per
  line formatted in CSV (fields are separated with commas). For example:

    ```
    $ cat AUTHORS
    Jean Dupont,00010001,jdupont@ucdavis.edu
    Marc Durand,00010002,mdurand@ucdavis.edu
    ```

- `REPORT2.md`: a
  [markdown-formatted](https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet)
  file containing a description of your submission.

    This file should explain your design choices, how you tested your project,
    the sources that you may have used to complete this project, etc. and any
    other relevant information.

## Git

Your submission must be under the shape of a Git bundle. In your git repository,
type in the following command (your work must be in the branch `master`):

```
$ git bundle create uthread.bundle master
```

It should create the file `uthread.bundle` that you will submit via `handin`.

You can make sure that your bundle has properly been packaged by extracting it
in another directory and verifying the log:

```
$ cd /path/to/tmp/dir
$ git clone /path/to/uthread.bundle -b master uthread
$ cd uthread
$ git log
...
```

## Handin

Your Git bundle, as created above, is to be submitted with `handin` from one of
the CSIF computers:

```
$ handin cs150 p3 uthread.bundle
Submitting uthread.bundle... ok
$
```

You can verify that the bundle has been properly submitted:

```
$ handin cs150 p3
The following input files have been received:
...
$
```

# Academic integrity

You are expected to write this project from scratch, thus avoiding to use any
existing source code available on the Internet. You must specify in your
`REPORT2.md` file any sources of code that you or your partner have viewed to
help you complete this project. All class projects will be submitted to MOSS to
determine if pairs of students have excessively collaborated with other pairs.
Excessive collaboration, or failure to list external code sources will result in
the matter being transferred to Student Judicial Affairs.

