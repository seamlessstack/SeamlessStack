mkdir /tmp/one /tmp/sfs
export LD_LIBRARY_PATH=build/lib:oss_install/lib
valgrind --tool=callgrind ./sfs 127.0.0.1,/var/tmp,rw,10 /tmp/one
