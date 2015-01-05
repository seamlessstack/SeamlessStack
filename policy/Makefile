
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
