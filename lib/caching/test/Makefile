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

