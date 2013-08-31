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

TOPDIR = ..
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I $(TOPDIR)/common/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I ../oss/jerasure_lib
CFLAGS += -D_GNU_SOURCE -g
CFLAGS += -Wall
OSS_DIR = $(TOPDIR)/oss/
OSS_LIBDIR = $(TOPDIR)/oss_install/lib
LIB = -lpthread -lsstack_common -ljerasure -lrt
LDFLAGS = -L $(TOPDIR)/common/ -L $(OSS_LIBDIR)

SFSD_OBJS = src/main.o src/sfsd.o src/sfsd_signals.o src/sfsd_threads.o \
	    src/sfsd_ops.o src/sfsd_transport.o src/sfsd_erasure.o \

all:: sfsd

sfsd: $(SFSD_OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(SFSD_OBJS) $(LIB)

clean:
	rm -f sfsd src/*.o
