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

JERASUREDIR = jerasure_lib
MEMCACHEDDIR = libmemcached-1.0.17
MONGOCDIR = mongo-c-driver
LZ4DIR = lz4
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
	$(MAKE) -C $(MONGOCDIR)
	$(CP) $(JERASUREDIR)/*.so $(OSS_LIBSDIR)
	$(CP) $(MONGOCDIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(LZ4DIR) 
	$(CP) $(LZ4DIR)/*.so $(OSS_LIBSDIR)

$(CONFIGURE):
	$(CD) $(MEMCACHEDDIR) && ./configure --prefix=$(OSS_INSTALL_DIR)

clean:
	$(MAKE) -C $(JERASUREDIR) clean
	$(MAKE) -C $(MEMCACHEDDIR) clean
	$(MAKE) -C $(MONGOCDIR) clean
	$(MAKE) -C $(LZ4DIR) clean
