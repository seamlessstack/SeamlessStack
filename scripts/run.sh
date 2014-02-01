mkdir /tmp/sfs /tmp/one
export LD_LIBRARY_PATH=build/lib:oss_install/lib
# Start sfs
build/bin/sfs 127.0.0.1,/var/tmp,rw,10 /tmp/one
# Start sfsd
ipaddr=` ifconfig eth0 | awk -F':' '/inet addr/&&!/127.0.0.1/{split($2,_," ");print _[1]}'`
build/bin/sfsd /tmp/sfsd $ipaddr

