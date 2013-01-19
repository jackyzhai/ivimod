#!/bin/sh
echo Inserting kernel module ...
insmod $IVIPATH/ivi_stateful.ko || echo ivi_stateful.ko failed.
insmod $IVIPATH/ivi_portmapping.ko || echo ivi_portmapping.ko failed.
insmod $IVIPATH/ivi_partialstate.ko
insmod $IVIPATH/ivi.ko || echo ivi.ko failed.
echo Done.

echo Setting up ivi0 and ivi1 interfaces ...
ip link set ivi0 up
ip link set ivi1 up
echo Done.

echo Enable forwarding ...
echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
echo 1 > /proc/sys/net/ipv4/conf/all/forwarding
echo Done.

echo Assigning addresses to ivi0 and ivi1 ...
ip addr add 10.0.0.1/24 dev ivi0 
ip -6 addr add fec0::1/64 dev ivi1 
ip link set mtu 1480 dev ivi0
echo Done.
