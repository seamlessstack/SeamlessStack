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
MKDIR = mkdir

all:
	$(MKDIR) -p $(OSS_INSTALL_DIR)
	$(MAKE) -C $(OSS)
	$(MAKE) -C $(MONGODIR)
	$(MAKE) -C $(MONGODRV)
	$(MAKE) -C $(POLICYDIR)
	$(MAKE) -C $(CLIDIR)
	$(MAKE) -C $(CACHINGDIR)
	$(MAKE) -C $(COMPRESSION_PLUGIN)
	$(MAKE) -C $(COMMONDIR)
	$(MAKE) -C $(SFSDDIR)
	$(MAKE) -C $(SFSDIR)

clean:
	$(MAKE) -C $(OSS) clean
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(MONGODRV) clean
	$(MAKE) -C $(POLICYDIR) clean
	$(MAKE) -C $(CLIDIR) clean
	$(MAKE) -C $(CACHINGDIR) clean
	$(MAKE) -C $(COMPRESSION_PLUGIN) clean
	$(MAKE) -C $(SFSDDIR) clean
	$(MAKE) -C $(SFSDIR) clean
	$(MAKE) -C $(COMMONDIR) clean
	$(RM) -rf $(OSS_INSTALL_DIR)
