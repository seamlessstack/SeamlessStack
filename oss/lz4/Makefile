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
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I include
LZ4_LIBNAME = liblz4.so
LZ4_OBJECTS = src/lz4.o src/lz4hc.o src/xxhash.o
LDFLAGS = -shared -o
CC = c99
RM = rm

all:: $(LZ4_LIBNAME)

$(LZ4_LIBNAME) : $(LZ4_OBJECTS)
	$(CC) $(LDFLAGS) $(LZ4_LIBNAME) $(LZ4_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(LZ4_OBJECTS) $(LZ4_LIBNAME)

