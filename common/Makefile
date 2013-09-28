#  
#  SEAMLESSSTACK CONFIDENTIAL
#  __________________________
#  
#   [2012] - [2013]  SeamlessStack Inc
#   All Rights Reserved.
#  
#  NOTICE:  All information contained herein is, and remains
#  the property of SeamlessStack Incorporated and its suppliers,
#  if any.  The intellectual and technical concepts contained
#  herein are proprietary to SeamlessStack Incorporated
#  and its suppliers and may be covered by U.S. and Foreign Patents,
#  patents in process, and are protected by trade secret or copyright law.
#  Dissemination of this information or reproduction of this material
#  is strictly forbidden unless prior written permission is obtained
#  from SeamlessStack Incorporated.
#

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
			  src/bds_slab/bds_sys.o
COMMON_OBJS += src/crc_pcl.o

all: $(COMMON_OBJS)
	$(CC)  $(LIB) $(LDFLAGS),$(SONAME) -o libsstack_common.so $(COMMON_OBJS)

src/crc_pcl.o:
	ln -s $(TOPDIR)/../arch/x86_64/crc_pcl.S src
	$(ASM) -f x64 -f elf64 -X gnu -g dwarf2 -D LINUX -o src/crc_pcl.o \
			src/crc_pcl.S
	$(RM) -f src/crc_pcl.S

clean:
	rm -f *.so src/*.o src/bds_slab/*.o
