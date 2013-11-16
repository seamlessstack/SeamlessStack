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

CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I ./include -I ../db/include/ -I ../common/include/ \
		  -I ../lib/validate/include/
POLICY_LIB_NAME = libpolicy.so
POLICY_OBJS = src/policy.o src/validate_policy.o
LDFLAGS = -shared -o
LIBS = $(shell pkg-config --libs fuse) -lpthread -lJudy 
CC = gcc
RM = rm


all:: $(POLICY_LIB_NAME)

$(POLICY_LIB_NAME): $(POLICY_OBJS)
	$(CC) $(LDFLAGS) $@ $(POLICY_OBJS) $(LIBS)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(POLICY_LIB_NAME) $(POLICY_OBJS)
