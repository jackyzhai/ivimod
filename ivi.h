#ifndef _IVI_H
#define _IVI_H
/* ivi.h
 * Definitions
 * IVI Project, CERNET
 * old version as mapping.h
 * create:                                2008.11.05  Hong Zhang
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h>      /* printk() */
#include <linux/slab.h>        /* kmalloc() */
#include <linux/errno.h>       /* error codes */
#include <linux/types.h>       /* size_t */
#include <linux/interrupt.h>   /* mark_bh */

#include <asm/atomic.h>
#include <asm/checksum.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/if_ether.h>
#include <linux/ip.h>          /* struct iphdr */
#include <linux/ipv6.h>        /* struct ipv6hdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/udp.h>         /* struct udphdr */
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/skbuff.h>
#include <net/route.h>
#include <net/ipv6.h>          /* struct frag_hdr */
#include <net/ip6_route.h>

#include <linux/ioctl.h>
#include <linux/sockios.h>

#include "ivimap.h"
#define CONFIG_IVI_HGI4
#ifdef CONFIG_IVI_HGI4
#include "ivi_portmapping.h"
#endif
#include "ivi_stateful.h"

/* These are the flags in the statusword */
#define IVI_SEND	 1
#define IVI_RCV		 2

/* Default timeout period */
#define IVI_TIMEOUT 5   /* In jiffies */

extern struct net_device *ivi_devs[];

#define IVI_DATA_LEN ETH_DATA_LEN+100

#undef  MAPPING_FRAGMENTATION
#undef  MAPPING_EXT_HEADER



/*VI->IV functions*/

extern ssize_t iphdr6to4(struct ivi_buff *ivib);
extern ssize_t ip6to4(struct ivi_buff *ivib);
extern ssize_t icmp6to4(struct ivi_buff *ivib);
extern ssize_t tcp6to4(struct ivi_buff *ivib);
extern ssize_t udp6to4(struct ivi_buff *ivib);
extern int ip6_mapping(struct sk_buff *skb, __u8 *newdata, struct mapping_entry6 * entry);

/*IV->VI fuctions*/

extern int iphdr4to6(struct ivi_buff *ivib);
extern ssize_t ip4to6(struct ivi_buff *ivib);
extern int icmp4to6(struct ivi_buff *ivib);
extern ssize_t tcp4to6(struct ivi_buff *ivib);
extern ssize_t udp4to6(struct ivi_buff *ivib);
extern int ip_mapping(struct sk_buff *skb, __u8 *newdata, struct mapping_entry4 * entry);
#endif
