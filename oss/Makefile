JERASUREDIR = jerasure_lib
MEMCACHEDDIR = libmemcached-1.0.17
MONGOCDIR = mongo-c-driver
CONFIGURE = conf
OSS_DIR = $(PWD)
OSS_LIBSDIR = $(OSS_DIR)/oss_libs
RM = rm
CC = gcc
CP = cp
MKDIR = mkdir
CD = cd

all: $(CONFIGURE)
	$(MKDIR) -p $(OSS_LIBSDIR)
	$(MAKE) -C $(JERASUREDIR)
	$(MAKE) -C $(MEMCACHEDDIR)
	$(MAKE) -C $(MEMCACHEDDIR) install
	$(CP) $(JERASUREDIR)/*.so $(OSS_LIBSDIR)/lib
	$(CP) $(MONGOCDIR)/*.so $(OSS_LIBSDIR)/lib

$(CONFIGURE):
	$(CD) $(MEMCACHEDDIR) && ./configure --prefix=$(OSS_LIBSDIR)

clean:
	$(MAKE) -C $(JERASUREDIR) clean
	$(MAKE) -C $(MEMCACHEDDIR) clean
	$(RM) -rf $(OSS_LIBSDIR)
