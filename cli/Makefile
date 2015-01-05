
TOPDIR=..
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I $(TOPDIR)/common/include -I include -I $(TOPDIR)/sfs/include
CFLAGS += -I $(TOPDIR)/policy/include
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS += -I $(TOPDIR)/cli/include
CFLAGS += -std=c99
CFLAGS += -fPIC
LDFLAGS = -shared -lc -Wl,-soname
LIBS = -lpthread -L.
SONAME = libsfscli.so
RM = rm
CC = c99

LOG_DECODE_OBJS = src/sstack_log_decode.o
SFSCLI_OBJS = src/sfscli.o 
SFSCLID_OBJS = src/sfscli_clid.o

SFSCLILIBOBJS = src/sfscli_policy.o  src/sfscli_serdes.o src/sfscli_storage.o
SFSCLILIBOBJS += src/sfscli_sfsd.o src/sfscli_license.o


all:: libs sstack_log_decode sfsclid sfscli

libs: $(SFSCLILIBOBJS)
	$(CC) $(LIB) $(LDFLAGS),$(SONAME) -o libsfscli.so $(SFSCLILIBOBJS)

sstack_log_decode: $(LOG_DECODE_OBJS)
	$(CC) -o $@ $(LOG_DECODE_OBJS) $(LIBS)

sfscli: $(SFSCLI_OBJS)
	$(CC) -o $@ $(SFSCLI_OBJS) $(LIBS) -lsfscli

sfsclid: $(SFSCLID_OBJS)
	$(CC) -o $@ $(SFSCLID_OBJS) $(LIBS) -lsfscli

clean:
	$(RM) -f sstack_log_decode sfscli src/*.o libsfscli.so

