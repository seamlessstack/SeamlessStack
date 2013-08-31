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

CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../include -I /usr/local/include
TARGET = test
OBJS = test.o
LDFLAGS = -o
CC = gcc
RM = rm
LIBS = -L.. -lcache

all:: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $(TARGET) $(OBJS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f *.o $(TARGET)

