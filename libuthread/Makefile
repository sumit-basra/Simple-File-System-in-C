CFLAGS  = -g -Wall -Werror

all: libuthread.a

libuthread.a: queue.o context.o uthread.o fs.o disk.o
	ar crs libuthread.a queue.o context.o uthread.o fs.o disk.o

queue.o: queue.c queue.h 
	gcc $(CFLAGS) -c queue.c

context.o: context.c context.h
	gcc $(CFLAGS) -c context.c

uthread.o: uthread.c uthread.h
	gcc $(CFLAGS) -c uthread.c

fs.o: fs.c fs.h
	gcc $(CFLAGS) -c fs.c

disk.o: disk.c disk.h
	gcc $(CFLAGS) -c disk.c

clean:
	rm -f *.o libuthread.a