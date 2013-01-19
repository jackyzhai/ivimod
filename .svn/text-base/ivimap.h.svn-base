/* ivi.h
 * Definitions for IVIMAP command
 * IVI Project, CERNET
 * create:                                2009.06.29  Hong Zhang
 */

#ifndef _IVIMAP_H
#define _IVIMAP_H
#include <linux/types.h>
#include <linux/sockios.h>

#ifdef __KERNEL__
#include <linux/in.h>
#else
#include <netinet/in.h>
#endif

#define IVI_IOC_RESET      SIOCDEVPRIVATE
#define IVI_IOC_ADD4      (SIOCDEVPRIVATE+1)
#define IVI_IOC_DEL4      (SIOCDEVPRIVATE+2)
#define IVI_IOC_CONFIG4   (SIOCDEVPRIVATE+3)
#define IVI_IOC_GET4      (SIOCDEVPRIVATE+4)
#define IVI_IOC_ADD6      (SIOCDEVPRIVATE+5)
#define IVI_IOC_DEL6      (SIOCDEVPRIVATE+6)
#define IVI_IOC_CONFIG6   (SIOCDEVPRIVATE+7)
#define IVI_IOC_GET6      (SIOCDEVPRIVATE+8)


/*IVI ioctl err*/
#define IVI_IOC_ERR_EXIST         1
#define IVI_IOC_ERR_NOEXIST       2

/*mapping_entry mode*/
#define IVI_6HOST_MULTIPLEX 0x01
#define IVI_4HOST_MULTIPLEX 0x02
#define IVI_HGI4 0x04
#define IVI_6HOST_STATEFUL 0x08
#define IVI_MSS_CLAMPING 0x10
#define IVI_TRACERT_ADDR_TRANS 0x20
#define IVI_PING_BEACON4 0x40
#define IVI_PING_BEACON6 0x80
#define IVI_PS4	0x0100
#define IVI_PS6 0x0200

struct control
{
	struct in_addr flag;
	int mss;
	int icmp;
};

struct mapping_entry4
{
	struct in_addr index;
	struct in6_addr srcp;
	struct in6_addr dstp;
	__u8 srcl;
	__u8 dstl;
	__u16 src_ratio;
	__u16 dst_ratio;
	__u16 hgi4_offset;
	__u8 error;
	__u16 mode;
	__be32 hgi_prefix; /*prefix for hgi connected network*/
	struct control ctl;
};

struct mapping_entry6
{
	struct in6_addr index;
	__u8 srcl;
	__u8 dstl;
	__u8 error;
	__u16 mode;
	__u32 icmp_ttl_prefix; /*used for icmp ttl exceed message*/
	struct control ctl;
};

struct ivi_buff
{
	struct iphdr *iphdr4;
	struct ipv6hdr *iphdr6;
	struct iphdr *iphdr4_embed;
	struct ipv6hdr *iphdr6_embed;
	struct mapping_entry4 *entry4;
	struct mapping_entry6 *entry6;
	__u8 *ptr4;
	__u8 *ptr6;
	__u8 *end4;
	__u8 *end6;
	/*the following two is used when port mapping is enabled,
	 * in order to calculate the correct checksum*/
	__u16 sport4;
	__u16 sport6;
	__u16 dport4;
	__u16 dport6;
	int embeded;
	int fragmented;
};

#endif
