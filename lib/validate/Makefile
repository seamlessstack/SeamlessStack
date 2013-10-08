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
CFLAGS += -I ./include 
CFLAGS += -D_GNU_SOURCE -fPIC
VALIDATE_LIB_NAME = libvalidate.so
VALIDATE_OBJECTS = src/sfs_validate.o
LDFLAGS = -shared -o
CC = gcc
LIBS =

all:: $(VALIDATE_LIB_NAME)

$(VALIDATE_LIB_NAME) : $(VALIDATE_OBJECTS)
	$(CC) $(LDFLAGS) $(VALIDATE_LIB_NAME) $(VALIDATE_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f src/*.o $(VALIDATE_LIB_NAME)

