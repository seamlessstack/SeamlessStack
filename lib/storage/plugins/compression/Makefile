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

TOPDIR = ../../../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I$(TOPDIR)/policy/include
COMPRESSION_LIB_NAME = libcompressplugin.so
COMPRESSION_OBJECTS = sstack_compression_plugin.o
LDFLAGS = -shared -o
LIBS = -llzma -lpthread
CC = c99
RM = rm

all:: $(COMPRESSION_LIB_NAME)

$(COMPRESSION_LIB_NAME) : $(COMPRESSION_OBJECTS)
	$(CC) $(LDFLAGS) $(COMPRESSION_LIB_NAME) $(COMPRESSION_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(COMPRESSION_OBJECTS) $(COMPRESSION_LIB_NAME)

