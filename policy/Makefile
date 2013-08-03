#CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include 
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DFUSE_USE_VERSION=26
CFLAGS += -DMONGO_HAVE_STDINT -Wall
LIB = $(shell pkg-config --libs fuse) -lpthread -lJudy 

POLICY_OBJS = src/policy.o

all:: policy

policy: $(POLICY_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(POLICY_OBJS) $(LIB)

clean:
	rm -f policy src/*.o
