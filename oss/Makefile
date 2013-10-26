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
MEMCACHEDDIR = libmemcached-1.0.8
MONGOCDIR = mongo-c-driver
LZ4DIR = lz4
CONFIGURE = conf
OSS_DIR = $(PWD)
OSS_INSTALL_DIR = $(OSS_DIR)/oss_install
OSS_LIBSDIR = $(OSS_INSTALL_DIR)/lib
OPENSSL_DIR = openssl-1.0.1c
RBTREE_DIR = rbtree
RM = rm
CC = gcc
CP = cp
MKDIR = mkdir
CD = cd
SED = sed
MV = mv
CP = cp

all: $(CONFIGURE)
	$(MAKE) -C $(JERASUREDIR)
	$(MAKE) -C $(MEMCACHEDDIR)
	$(MAKE) -C $(MEMCACHEDDIR) install
	$(MAKE) -C $(MONGOCDIR)
	$(CP) $(JERASUREDIR)/*.so $(OSS_LIBSDIR)
	$(CP) $(MONGOCDIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(LZ4DIR) 
	$(CP) $(LZ4DIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(OPENSSL_DIR)
	$(CP) $(OPENSSL_DIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(RBTREE_DIR)
	$(CP) $(RBTREE_DIR)/*.so $(OSS_LIBSDIR)

$(CONFIGURE):
	$(CD) $(MEMCACHEDDIR) && ./configure --prefix=$(OSS_INSTALL_DIR)
	$(CD) $(OPENSSL_DIR) && ./config shared

clean:
	$(MAKE) -C $(JERASUREDIR) clean
	$(MAKE) -C $(MEMCACHEDDIR) clean
	$(MAKE) -C $(MONGOCDIR) clean
	$(MAKE) -C $(LZ4DIR) clean
	$(MAKE) -C $(OPENSSL_DIR) clean
	$(MAKE) -C $(RBTREE_DIR) clean
