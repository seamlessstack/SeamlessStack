#!/bin/bash
user_id=`id -u`
if [ $user_id -ne 0 ];
then
	echo "Run this script with sudo"
	exit -1;
fi

kill -9 `pidof sfs`
rm -f /tmp/sfs/*
umount /tmp/one
dmesg -c
