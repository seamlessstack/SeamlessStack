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

MONGODIR = db/mongo
SFSDIR = sfs
MONGODRV = oss/mongo-c-driver
OSS = oss
POLICYDIR = policy
CLIDIR	= cli
SFSDDIR = sfsd
COMMONDIR = common
CACHINGDIR = lib/caching
COMPRESSION_PLUGIN = lib/storage/plugins/compression
OSS_INSTALL_DIR = $(PWD)/oss_install
PROTOBUF_DIR = $(PWD)/protobuf-template/proto
SERDES_DIR = $(PWD)/lib/serdes
VALIDATE_DIR= $(PWD)/lib/validate
MKDIR = mkdir
PROTOC = protoc-c
LN = ln -sf
CSCOPE = cscope
CTAGS = ctags
RM = rm

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
	$(MAKE) -C $(COMPRESSION_PLUGIN)
	$(MAKE) -C $(SERDES_DIR)
	$(MAKE) -C $(COMMONDIR)
	$(MAKE) -C $(VALIDATE_DIR)
	$(MAKE) -C $(SFSDDIR)
	$(MAKE) -C $(SFSDIR)

cscope:
	$(RM) -f cscope.out
	$(CSCOPE) -bR

ctags:
	$(RM) -f tags
	$(CTAGS) -R

clean:
	$(MAKE) -C $(OSS) clean
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(MONGODRV) clean
	$(MAKE) -C $(POLICYDIR) clean
	$(MAKE) -C $(CLIDIR) clean
	$(MAKE) -C $(CACHINGDIR) clean
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
