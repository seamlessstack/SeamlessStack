MONGODIR = db/mongo
SFSDIR = sfs
MONGODRV = oss/mongo-c-driver
POLICYDIR = policy

all:
	$(MAKE) -C $(MONGODIR)
	$(MAKE) -C $(MONGODRV)
	$(MAKE) -C $(SFSDIR)
	$(MAKE) -C $(POLICYDIR)

clean:
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(SFSDIR) clean
	$(MAKE) -C $(MONGODRV) clean
	$(MAKE) -C $(POLICYDIR) clean
