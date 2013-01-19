#ifndef IVI_PARTIALSTATE4_H
#define IVI_PARTIALSTATE4_H

#include <linux/in.h>
#include <linux/list.h>

struct ivi_ps{
	struct list_head addrlist_entry;
	struct in_addr addr;
	__u8 protocol;
	unsigned long oldportbitmap[8192/sizeof(unsigned long)];
	unsigned long newportbitmap[8192/sizeof(unsigned long)];
	struct list_head portlist;
	spinlock_t addrlock;
};

struct portmapping
{
	struct list_head portlist_entry;
	__u16 oldport;
	__u16 newport;
	__u64 lasttime;
};

extern int ivi_ps_getnewp(__u16 port, __be32 addr, __u16 ratio, __u16 offset, __u8 protocol, __u8 version);
extern int ivi_ps_getoldp(__u16 port, __be32 addr, __u16 ratio, __u16 offset, __u8 protocol, __u8 version);
#endif
