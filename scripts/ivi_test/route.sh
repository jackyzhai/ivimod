#!/bin/sh
echo Setting routes for ipv4 and ipv6 ...
ip route add 1.1.2.0/24 via 10.0.0.2 dev ivi0 
ip route add 1.1.3.0/24 via 10.0.0.2 dev ivi0 
# ip -6 route add 2001:da8:a123:212::/64 dev eth0.0 
ip -6 route add 2001:da8:ff01:0101::/64 via fec0::2 dev ivi1
ip -6 route add 2001:da8:ff01:0103::/64 dev eth0
echo Done.
