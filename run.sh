mkdir /tmp/sfs /tmp/one
export LD_LIBRARY_PATH=build/lib:oss_install/lib
# Start sfs
build/bin/sfs 127.0.0.1,/var/tmp,rw,10 /tmp/one
# Start sfsd
build/bin/sfsd /tmp/sfsd 127.0.0.1

