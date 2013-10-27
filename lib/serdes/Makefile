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

TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -fPIC -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS += -I$(TOPDIR)/common/include -I$(TOPDIR)/protobuf-template/proto
CFLAGS += -Iinclude -I$(TOPDIR)/common/include/bds_slab
CFLAGS += -I$(TOPDIR)/policy/include
SERDES_LIB_NAME = libserdes.so
SERDES_OBJECTS = src/sstack_serdes.o
SERDES_OBJECTS += $(TOPDIR)/protobuf-template/proto/jobs.pb-c.o
LDFLAGS = -shared -o
LIBS = -lprotobuf-c
CC = gcc
RM = rm

all:: $(SERDES_LIB_NAME)

$(SERDES_LIB_NAME) : $(SERDES_OBJECTS)
	$(CC) $(LDFLAGS) $(SERDES_LIB_NAME) $(SERDES_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(SERDES_OBJECTS) $(SERDES_LIB_NAME)

