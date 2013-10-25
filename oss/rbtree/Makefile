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
CFLAGS = -Wall -pedantic
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -fPIC
RBTREE_OBJS = red_black_tree.o stack.o misc.o
CC = gcc
RBTREE_LIB_NAME = librbtree.so
LDFLAGS = -shared -o


all: $(RBTREE_LIB_NAME)

$(RBTREE_LIB_NAME): $(RBTREE_OBJS)
		$(CC) $(LDFLAGS) $(RBTREE_LIB_NAME) $(RBTREE_OBJS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f *.o $(RBTREE_LIB_NAME)

