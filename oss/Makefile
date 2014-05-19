#  
#  SEAMLESSSTACK CONFIDENTIAL
#  __________________________
#  
#   [2012] - [2014]  SeamlessStack Inc
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

JERASUREDIR = jerasure2.0
GF_COMPLETEDIR = gf-complete
MEMCACHEDDIR = libmemcached-1.0.8
MONGOCDIR = mongo-c-driver
LZ4DIR = lz4
CONFIGURE = conf
OSS_DIR = $(PWD)
OSS_INSTALL_DIR = $(OSS_DIR)/oss_install
OSS_LIBSDIR = $(OSS_INSTALL_DIR)/lib
OSS_INCLUDEDIR = $(OSS_INSTALL_DIR)/include
OPENSSL_DIR = openssl-1.0.1g
GFP = $(shell pwd)/gf-complete
RBTREE_DIR = rbtree
CLISH_DIR = clish-0.7.3
ROCKSDB_DIR = rocksdb
CZMQ_DIR = czmq
RM = rm
CC = gcc
CP = cp
MKDIR = mkdir -p
CD = cd
SED = sed
MV = mv
CP = cp
LN = ln

all: $(CONFIGURE)
	$(MAKE) -C $(JERASUREDIR)
	$(MAKE) -C $(MEMCACHEDDIR)
	$(MAKE) -C $(MEMCACHEDDIR) install
	$(MAKE) -C $(MONGOCDIR)
	$(CP) $(JERASUREDIR)/src/.libs/*.so $(OSS_LIBSDIR)
	$(LN) -sf $(OSS_LIBSDIR)/libJerasure.so $(OSS_LIBSDIR)/libJerasure.so.2	
	$(CP) $(MONGOCDIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(LZ4DIR) 
	$(CP) $(LZ4DIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(OPENSSL_DIR)
	$(CP) $(OPENSSL_DIR)/*.so $(OSS_LIBSDIR)
	$(MKDIR) $(OSS_INCLUDEDIR)/openssl
	$(CP)  $(OPENSSL_DIR)/include/openssl/* $(OSS_INCLUDEDIR)/openssl
	$(MAKE) -C $(RBTREE_DIR)
	$(CP) $(RBTREE_DIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(CLISH_DIR)
	$(MAKE) -C $(ROCKSDB_DIR) shared_lib
	$(CP) $(ROCKSDB_DIR)/*.so $(OSS_LIBSDIR)
	$(MAKE) -C $(CZMQ_DIR)
	$(CP) $(CZMQ_DIR)/src/.libs/*.so $(OSS_LIBSDIR)

$(CONFIGURE): gf_comp jerasure_touch
	$(CD) $(MEMCACHEDDIR) && ./configure --prefix=$(OSS_INSTALL_DIR)
	$(CD) $(OPENSSL_DIR) && ./config shared
	$(CD) $(JERASUREDIR) && ./configure LDFLAGS=-L$(GFP)/src/.libs \
			CPPFLAGS=-I$(GFP)/include
	$(CD) $(CZMQ_DIR) && ./autogen.sh
	$(CD) $(CZMQ_DIR) && ./configure

gf_comp:
	$(MAKE) -C $(GF_COMPLETEDIR)

jerasure_touch:
	$(CD) $(JERASUREDIR) && touch configure.ac aclocal.m4 configure \
				Makefile.am Makefile.in

clean:
	$(MAKE) -C $(JERASUREDIR) clean
	$(MAKE) -C $(MEMCACHEDDIR) clean
	$(MAKE) -C $(MONGOCDIR) clean
	$(MAKE) -C $(LZ4DIR) clean
	$(MAKE) -C $(OPENSSL_DIR) clean
	$(MAKE) -C $(RBTREE_DIR) clean
	$(MAKE) -C $(CLISH_DIR) clean
	$(MAKE) -C $(GF_COMPLETEDIR) clean
	$(MAKE) -C $(ROCKSDB_DIR) clean
	$(MAKE) -C $(CZMQ_DIR) clean
