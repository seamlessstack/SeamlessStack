CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I ../common/include
CFLAGS += -D_GNU_SOURCE 
CFLAGS += -Wall
LIB = -lpthread -lsstack_common
LDFLAGS = -L ../common/

SFSD_OBJS = src/main.o src/sfsd.o src/sfsd_signals.o \
	    src/sfsd_threads.o src/sfsd_ops.o \
	    src/sfsd_transport.o \

all:: sfsd

sfsd: $(SFSD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SFSD_OBJS) $(LIB)

clean:
	rm -f sfsd src/*.o
