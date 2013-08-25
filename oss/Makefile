JERASUREDIR = jerasure_lib
MEMCACHEDDIR = libmemcached-1.0.17
MONGOCDIR = mongo-c-driver
CONFIGURE = conf
OSS_DIR = $(PWD)
OSS_INSTALL_DIR = $(OSS_DIR)/oss_install
OSS_LIBSDIR = $(OSS_INSTALL_DIR)/lib
RM = rm
CC = gcc
CP = cp
MKDIR = mkdir
CD = cd

all: $(CONFIGURE)
	$(MAKE) -C $(JERASUREDIR)
	$(MAKE) -C $(MEMCACHEDDIR)
	$(MAKE) -C $(MEMCACHEDDIR) install
	$(CP) $(JERASUREDIR)/*.so $(OSS_LIBSDIR)
	$(CP) $(MONGOCDIR)/*.so $(OSS_LIBSDIR)

$(CONFIGURE):
	$(CD) $(MEMCACHEDDIR) && ./configure --prefix=$(OSS_INSTALL_DIR)

clean:
	$(MAKE) -C $(JERASUREDIR) clean
	$(MAKE) -C $(MEMCACHEDDIR) clean
