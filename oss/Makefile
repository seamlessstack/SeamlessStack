JERASUREDIR = jerasure_lib
all:	
	mkdir -p oss_libs
	$(MAKE) -C $(JERASUREDIR)
	cp $(JERASUREDIR)/*.so oss_libs/
	
clean:
	$(MAKE) -C $(JERASUREDIR) clean
	rm -rf oss_libs
