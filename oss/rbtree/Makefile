
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

