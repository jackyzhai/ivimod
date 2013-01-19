/*
 * =====================================================================================
 *
 *       Filename:  ivi_stateful.h
 *
 *    Description:  ivi_stateful for v4 server(seems like that)
 *
 *        Version:  1.0
 *        Created:  09/12/2009 12:11:03 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhai Yu (Jacky), jacky.zhai@gmail.com
 *        Company:  THU
 *
 * =====================================================================================
 */
#ifndef IVI_STATEFUL_H
#define IVI_STATEFUL_H
#include <linux/in6.h>
#include <linux/spinlock.h>
/*IVI 1:N State Table*/

struct t46_entry
{
	struct in6_addr addr;
	__u32 lastuse;
	int valid;
	spinlock_t t46lock;
};
extern int ivi_stateful_get6(__u32 index, struct in6_addr *res);
extern __u32 ivi_stateful_get4(const struct in6_addr *in6);
#endif
