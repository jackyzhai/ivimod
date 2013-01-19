/* table.c
 * 1:N State Table Implementation
 * 1:N Stateless Port Collision Avoidance Table Implementation
 * DO NOT adjust system time while running
 * IVI Project, CERNET
 * create:                                2008.11.05  Hong Zhang
 */

#include "ivi_stateful.h"
#include <linux/module.h>
#include <linux/jiffies.h>
MODULE_AUTHOR("Hong Zhang & Zhai Yu");
MODULE_LICENSE("Dual BSD/GPL");
#define IVI_MAPPINGTABLE_SIZE 65536
#define IVI_STATEFUL_TIMEOUT 5000
static struct t46_entry t46[IVI_MAPPINGTABLE_SIZE];
static int init_stateful_mapping(void)
{
	int i;
	for (i = 0; i < IVI_MAPPINGTABLE_SIZE; i++){
		t46[i].valid = 0;
		spin_lock_init(&(t46[i].t46lock));
	}
	return 0;
}

__u16 hash6 ( const struct in6_addr * in6 )
{
	int i = 0;
	__u16 total = 0;
	for ( ; i < 16; total+=(in6->s6_addr[i++]) );
	return total;
}

int ivi_stateful_get6 ( __u32 index, struct in6_addr *res)
{
	unsigned long flags;
	spin_lock_irqsave(&(t46[index].t46lock), flags);
	if (t46[index].valid == 0){
		spin_unlock_irqrestore(&(t46[index].t46lock), flags);
		return -1;
	}
	t46[index].lastuse = jiffies;
	memcpy(res, &(t46[index].addr), sizeof(struct in6_addr));
	spin_unlock_irqrestore(&(t46[index].t46lock), flags);
	return 0;
}
EXPORT_SYMBOL(ivi_stateful_get6);

__u32 ivi_stateful_get4 ( const struct in6_addr * in6 )
{
	__u16 hash = hash6(in6);
	__u32 time = jiffies;
	__u16 tmp = hash;
	unsigned long flags;
	while (tmp != hash + 100)
	{
		spin_lock_irqsave(&(t46[tmp].t46lock), flags);
		if(t46[tmp].valid == 0 || !strncmp((char *)in6, (char *)&(t46[tmp].addr), sizeof(struct in6_addr)) \
			       	|| (time - t46[tmp].lastuse) > IVI_STATEFUL_TIMEOUT)
			break;
		spin_unlock_irqrestore(&(t46[tmp].t46lock), flags);
		tmp ++;
	}
	if (tmp == hash + 100)
		return 0;
	t46[hash].valid = 1;
	t46[hash].addr = *in6;
	t46[hash].lastuse = time;
	spin_unlock_irqrestore(&(t46[tmp].t46lock), flags);
	return hash;
}
EXPORT_SYMBOL(ivi_stateful_get4);

void exit_stateful_mapping(void)
{
	printk("ivi stateful module for IPv4 server\
		       	is removed\n");
}
module_init(init_stateful_mapping);
module_exit(exit_stateful_mapping);
