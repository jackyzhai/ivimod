#!/bin/sh
echo Assigning addresses to lan interface ...
ip addr add 10.2.32.0/24 dev ivi0 || echo "assign ipv4 addr: failed"
ip -6 addr add 2001:da9:ff01:102:140::/64 dev ivi1 || echo "assign ipv6 addr: failed"
echo Done.

