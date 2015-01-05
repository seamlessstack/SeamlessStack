
TOPDIR = ../
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I $(TOPDIR)/common/include -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I $(TOPDIR)/policy/include
CFLAGS += -D_GNU_SOURCE -D_LINUX_ -DUSE_OS_INFO -D_x86_64_
CFLAGS += -fPIC
CFLAGS += -DSSE4_CRC
LIB = -lpthread -lrt
LDFLAGS = -shared -lc -Wl,-soname
SONAME = libsstack_common.so
ASM = yasm
LN = ln
RM = rm

COMMON_OBJS = src/sstack_thread_pool.o src/sstack_transport_tcp.o \
			  src/sstack_checksum.o src/bds_slab/bds_slab.o \
			  src/bds_slab/bds_sys.o src/sstack_transport.o \
			  src/sstack_helper.o

COMMON_OBJS += src/crc_pcl.o

all: $(COMMON_OBJS)
	$(CC)  $(LIB) $(LDFLAGS),$(SONAME) -o libsstack_common.so $(COMMON_OBJS)

src/crc_pcl.o:
	ln -sf $(TOPDIR)/../arch/x86_64/crc_pcl.S src
	$(ASM) -f x64 -f elf64 -X gnu -g dwarf2 -D LINUX -o src/crc_pcl.o \
			src/crc_pcl.S
	$(RM) -f src/crc_pcl.S

clean:
	rm -f *.so src/*.o src/bds_slab/*.o
