TARGET  := libuthread.a
OBJS    := queue.o disk.o fs.o context.o uthread.o

CC      := gcc 
CFLAGS  := -Werror 
CFLAGS  := -Wall 
CFLAGS  += -g 

LIBFLAGS = -rcs

ifneq ($(V),1) 
Q = @ 
endif

DEPS := $(patsubst %.o,%.d,$(OBJS)) 
-include $(DEPS)

all: $(TARGET) $(OBJS)

libuthread.a: $(OBJS)
	@echo "AR $@"
	@ar $(LIBFLAGS) $(TARGET) $^
    
%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< -MMD -MF $(@:.o=.d)

clean:
	@echo "CLEAN" 
	$(Q)rm -f $(TARGET) $(OBJS) $(DEPS)