
MONGODIR = db/mongo
SFSDIR = sfs
MONGODRV = oss/mongo-c-driver

all:
	$(MAKE) -C $(MONGODIR)
	$(MAKE) -C $(MONGODRV)
	$(MAKE) -C $(SFSDIR)

clean:
	$(MAKE) -C $(MONGODIR) clean
	$(MAKE) -C $(SFSDIR) clean
	$(MAKE) -C $(MONGODRV) clean
