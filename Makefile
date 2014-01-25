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

MONGODIR = db/mongo
SFSDIR = sfs
MONGODRV = oss/mongo-c-driver
OSS = oss
POLICYDIR = policy
CLIDIR	= cli
SFSDDIR = sfsd
COMMONDIR = common
CACHINGDIR = lib/memcache
GENCACHEDIR = lib/cache
SSDCACHEDIR = lib/ssdcache
COMPRESSION_PLUGIN = lib/storage/plugins/compression
OSS_INSTALL_DIR = $(PWD)/oss_install
PROTOBUF_DIR = $(PWD)/protobuf-template/proto
SERDES_DIR = $(PWD)/lib/serdes
VALIDATE_DIR= $(PWD)/lib/validate
BUILDDIR = $(PWD)/build
BINDIR = $(BUILDDIR)/bin
LIBDIR = $(BUILDDIR)/lib
MKDIR = mkdir
PROTOC = protoc-c
LN = ln -sf
CSCOPE = cscope
CTAGS = ctags
RM = rm
CP = cp

all:
	$(MKDIR) -p $(OSS_INSTALL_DIR)
	$(MAKE) -C $(OSS)
	# Following dirty hack is because --proto_path does not work !!
	$(LN) $(PROTOBUF_DIR)/jobs.proto jobs.proto
	$(PROTOC) --c_out=$(PROTOBUF_DIR) jobs.proto
	$(RM) -f jobs.proto
	$(LN) $(PROTOBUF_DIR)/cli.proto cli.proto
	$(PROTOC) --c_out=$(PROTOBUF_DIR) cli.proto
	$(RM) -f cli.proto
	$(MAKE) -C $(MONGODIR)
	$(MAKE) -C $(MONGODRV)
	$(MAKE) -C $(POLICYDIR)
	$(MAKE) -C $(CLIDIR)
	$(MAKE) -C $(CACHINGDIR)
	$(MAKE) -C $(GENCACHEDIR)
	$(MAKE) -C $(SSDCACHEDIR)
	$(MAKE) -C $(COMPRESSION_PLUGIN)
	$(MAKE) -C $(SERDES_DIR)
	$(MAKE) -C $(COMMONDIR)
	$(MAKE) -C $(VALIDATE_DIR)
	$(MAKE) -C $(SFSDDIR)
	$(MAKE) -C $(SFSDIR)

install:
	$(MKDIR) -p $(BUILDDIR) $(BINDIR) $(LIBDIR)
	$(CP) $(MONGODIR)/*.so $(LIBDIR)
	$(CP) $(MONGODRV)/*.so $(LIBDIR)
	$(CP) $(POLICYDIR)/*.so $(LIBDIR)
	$(CP) $(CLIDIR)/*.so $(LIBDIR)
	$(CP) $(CLIDIR)/sfscli $(BINDIR)
	$(CP) $(CLIDIR)/sfsclid $(BINDIR)
	$(CP) $(CLIDIR)/sstack_log_decode $(BINDIR)
	$(CP) $(CACHINGDIR)/*.so $(LIBDIR)
	$(CP) $(GENCACHEDIR)/*.so $(LIBDIR)
	$(CP) $(SSDCACHEDIR)/*.so $(LIBDIR)
	$(CP) $(COMPRESSION_PLUGIN)/*.so $(LIBDIR)
	$(CP) $(SERDES_DIR)/*.so $(LIBDIR)
	$(CP) $(COMMONDIR)/*.so $(LIBDIR)
	$(CP) $(VALIDATE_DIR)/*.so $(LIBDIR)
	$(CP) $(SFSDIR)/sfs $(BINDIR)
	$(CP) $(SFSDDIR)/sfsd $(BINDIR)
	$(LN) -s $(LIBDIR)/libbson.so $(LIBDIR)/libbson.so.0.7
	$(LN) -s $(LIBDIR)/libmongoc.so $(LIBDIR)/libmongoc.so.0.7

tags:
	$(RM) -f cscope.out
	$(CSCOPE) -bR
	$(RM) -f tags
	$(CTAGS) -R

clean:
	$(MAKE) -C $(OSS) clean
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(MONGODRV) clean
	$(MAKE) -C $(POLICYDIR) clean
	$(MAKE) -C $(CLIDIR) clean
	$(MAKE) -C $(CACHINGDIR) clean
	$(MAKE) -C $(GENCACHEDIR) clean
	$(MAKE) -C $(SSDCACHEDIR) clean
	$(MAKE) -C $(COMPRESSION_PLUGIN) clean
	$(MAKE) -C $(SERDES_DIR) clean
	$(MAKE) -C $(SFSDDIR) clean
	$(MAKE) -C $(SFSDIR) clean
	$(MAKE) -C $(COMMONDIR) clean
	$(MAKE) -C $(VALIDATE_DIR) clean
	$(RM) -rf $(OSS_INSTALL_DIR)
	$(RM) -f $(PROTOBUF_DIR)/jobs.pb-c.h  $(PROTOBUF_DIR)/jobs.pb-c.c
	$(RM) -f $(PROTOBUF_DIR)/cli.pb-c.h  $(PROTOBUF_DIR)/cli.pb-c.c
	$(RM) -f cscope.out
	$(RM) -f tags
	$(RM) -rf $(BUILDDIR)
