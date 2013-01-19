#!/bin/sh
echo Assigning addresses to lan interface ...
ip addr add 1.1.1.1/24 dev ivi0 || echo "assign ipv4 addr: failed"
ip -6 addr add 2001:da8:ff01:0102:140::/64 dev ivi1 || echo "assign ipv6 addr: failed"
echo Done.

