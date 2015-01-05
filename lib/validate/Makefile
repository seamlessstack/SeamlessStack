
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
LIBS = -L$(TOPDIR)/oss_install -lcrypto -lssl

all:: $(VALIDATE_LIB_NAME)

$(VALIDATE_LIB_NAME) : $(VALIDATE_OBJECTS)
	$(CC) $(LDFLAGS) $(VALIDATE_LIB_NAME) $(VALIDATE_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f src/*.o $(VALIDATE_LIB_NAME)

