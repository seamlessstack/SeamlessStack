MONGODIR = db/mongo
SFSDIR = sfs
MONGODRV = oss/mongo-c-driver
POLICYDIR = policy
SFSDDIR = sfsd

all:
	$(MAKE) -C $(MONGODIR)
	$(MAKE) -C $(MONGODRV)
	$(MAKE) -C $(POLICYDIR)
	$(MAKE) -C $(SFSDIR)
	$(MAKE) -C $(SFSDDIR)

clean:
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(MONGODRV) clean
	$(MAKE) -C $(POLICYDIR) clean
	$(MAKE) -C $(SFSDIR) clean
	$(MAKE) -C $(SFSDDIR) clean
