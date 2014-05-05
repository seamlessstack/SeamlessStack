mkdir /tmp/sfs /tmp/one /tmp/sfsd /tmp/junky /tmp/two
mkdir -p /var/sfs
export LD_LIBRARY_PATH=build/lib:oss_install/lib
ulimit -c unlimited
ulimit -s 16384

#Restart Mongodb
mkdir -p /var/lib/mongodb
chmod 777 /var/lib/mongodb
/etc/init.d/mongodb restart

# Start sfs
build/bin/sfs 127.0.0.1,/var/tmp,rw,10 /tmp/one
sleep 2
# Start sfsd
ipaddr=` ifconfig eth0 | awk -F':' '/inet addr/&&!/127.0.0.1/{split($2,_," ");print _[1]}'`
build/bin/sfsd /tmp/sfsd $ipaddr &
sleep 1
build/bin/sfsclid &
sleep 1
echo "/tmp/junky *(rw,sync,no_subtree_check)" >/etc/exports
exportfs -ra
build/bin/sfscli storage -a -A 127.0.0.1 -R /tmp/junky -P nfs -t hdd -s 1
