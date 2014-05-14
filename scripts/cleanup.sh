#!/bin/bash
user_id=`id -u`
if [ $user_id -ne 0 ];
then
	echo "Run this script with sudo"
	exit -1;
fi

umount /tmp/one
umount /tmp/dir*

pkill -9 sfs
pkill -9 sfsclid
pkill -9 sfsd
rm -rf /tmp/sfs /tmp/sfsd /tmp/dir*

rm -rf /tmp/one /tmp/two
rm -rf /tmp/junky
rm -rf /var/sfs

#Cleanup mongodb
rm -fr /var/lib/mongodb/*
rm -fr /var/lib/mongodb/journal/*
dmesg -c

#Cleanup core files
rm -r core
rm /core
