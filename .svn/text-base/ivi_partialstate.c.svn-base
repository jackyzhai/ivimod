#include "ivi.h"
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include "ivi_partialstate.h"
MODULE_AUTHOR("ZHAI YU");
MODULE_LICENSE("Dual BSD/GPL");

#define TIMEOUT 75000
LIST_HEAD(ps4_list);
spinlock_t ps4_lock;
LIST_HEAD(ps6_list);
spinlock_t ps6_lock;
static int init_ps(void)
{
	spin_lock_init(&ps4_lock);
	spin_lock_init(&ps6_lock);
	return 0;
}
static void free_ps(struct list_head *p)
{
	struct ivi_ps *pos, *tmp;
	struct portmapping *pmpos, *pmtmp;
	struct list_head *pmhead;
	list_for_each_entry_safe(pos, tmp, p, addrlist_entry){
		pmhead = &pos->portlist;
		list_for_each_entry_safe(pmpos, pmtmp, pmhead, portlist_entry){
			list_del(&(pmpos->portlist_entry));
			kfree(pmpos);
		}
		list_del(&pos->addrlist_entry);
		kfree(pos);
	}
}

static void exit_ps(void)
{
	free_ps(&ps4_list);
	free_ps(&ps6_list);
}
struct ivi_ps *ivi_ps_lookup_addr(__be32 addr, __u8 protocol, struct list_head *headp)
{
	struct ivi_ps *pos, *tmp;
	list_for_each_entry_safe(pos, tmp, headp, addrlist_entry){
		if(list_empty(&pos->portlist)){
			list_del(&pos->addrlist_entry);
			kfree(pos);
		}
		else if(pos -> addr.s_addr == addr && pos->protocol== protocol)
			return pos;
	}
	return NULL;
}
int ivi_ps_lookup_oldport(struct ivi_ps *p, __u16 newport, __u16 ratio, __u16 offset)
{
	struct portmapping *pos, *tmp;
	int res;
	__u64 time;
	unsigned long flags;
	spin_lock_irqsave(&(p->addrlock), flags);
	time = jiffies;
	if(test_bit(newport, p->newportbitmap)){
		list_for_each_entry_safe(pos, tmp, &(p->portlist), portlist_entry){
			if(time -  pos->lasttime> TIMEOUT){
				list_del(&(pos->portlist_entry));
				clear_bit(pos->oldport, p->oldportbitmap);
				clear_bit(pos->newport, p->newportbitmap);
				kfree(pos);
			}
			else if(pos -> newport == newport){
				pos->lasttime = time;
				res = pos->oldport;
				goto out;
			}
		}
	}
	/*no port mapped to oldport, generate a new portmapping*/
	if(!test_bit(newport, p->oldportbitmap)){
		res = newport;
		pos = kmalloc(sizeof(struct portmapping), GFP_ATOMIC);
		pos -> oldport = newport;
		pos -> newport = newport;
		pos -> lasttime = time;
		list_add(&(pos->portlist_entry), &(p->portlist));
		set_bit(newport, p->newportbitmap);
		set_bit(newport, p->oldportbitmap);
		goto out;
	}
	spin_unlock_irqrestore(&(p->addrlock), flags);
	return -1;
out:
	spin_unlock_irqrestore(&(p->addrlock), flags);
	return res;
}
int ivi_ps_lookup_newport(struct ivi_ps *p, __u16 oldport, __u16 ratio, __u16 offset)
{
	struct portmapping *pos, *tmp;
	int res;
	__u64 time;
	unsigned long flags;
	spin_lock_irqsave(&(p->addrlock), flags);
	time = jiffies;
	if(test_bit(oldport, p->oldportbitmap)){
		list_for_each_entry_safe(pos, tmp, &(p->portlist), portlist_entry){
			if(time - pos->lasttime > TIMEOUT){
				list_del(&(pos->portlist_entry));
				clear_bit(pos->oldport, p->oldportbitmap);
				clear_bit(pos->newport, p->newportbitmap);
				kfree(pos);
			}
			else if(pos -> oldport == oldport){
				pos->lasttime = time;
				res = pos->newport;
				goto out;
			}
		}
	}
	/*no port mapped to oldport, generate a new portmapping*/
	res = oldport - oldport % ratio + offset;
	for(; res < 65536; res += ratio){
		if (!test_bit(res, p->newportbitmap)){
			pos = kmalloc(sizeof(struct portmapping), GFP_ATOMIC);
			pos -> oldport = oldport;
			pos -> newport = res;
			pos -> lasttime = time;
			list_add(&(pos->portlist_entry), &(p->portlist));
			set_bit(res, p->newportbitmap);
			set_bit(oldport, p->oldportbitmap);
			goto out;
		}
	}
	spin_unlock_irqrestore(&(p->addrlock), flags);
	return -1;
out:
	spin_unlock_irqrestore(&(p->addrlock), flags);
	return res;
}
int ivi_ps_getnewp(__u16 oldport, __be32 addr, __u16 ratio, __u16 offset, __u8 protocol, __u8 version)
{
	struct ivi_ps *p;
	unsigned long flags;
	struct list_head *ps_list;
	spinlock_t *lockp;
	if(version == 4){
		ps_list = &ps4_list;
		lockp = &ps4_lock;
	}
	else if (version == 6){
		ps_list = &ps6_list;
		lockp = &ps6_lock;
	}
	else
		return -1;
	spin_lock_irqsave(lockp, flags);
        p = ivi_ps_lookup_addr(addr, protocol, ps_list);
	if (p == NULL){
		p = kmalloc(sizeof(struct ivi_ps), GFP_ATOMIC);
		p->addr.s_addr = addr;
		p->protocol = protocol;
		bitmap_zero(p->oldportbitmap, 65536);
		bitmap_zero(p->newportbitmap, 65536);
		INIT_LIST_HEAD(&p->portlist);
		spin_lock_init(&(p->addrlock));
		list_add(&(p->addrlist_entry), ps_list);
	}
	spin_unlock_irqrestore(lockp, flags);
	return ivi_ps_lookup_newport(p, oldport, ratio, offset);
}
EXPORT_SYMBOL(ivi_ps_getnewp);
int ivi_ps_getoldp(__u16 newport, __be32 addr, __u16 ratio, __u16 offset, __u8 protocol, __u8 version)
{
	struct ivi_ps *p;
	unsigned long flags;
	struct list_head *ps_list;
	spinlock_t *lockp;
	if(version == 4){
		ps_list = &ps4_list;
		lockp = &ps4_lock;
	}
	else if (version == 6){
		ps_list = &ps6_list;
		lockp = &ps6_lock;
	}
	else
		return -1;
	if (newport % ratio != offset)
		return -1;
	spin_lock_irqsave(lockp, flags);
        p = ivi_ps_lookup_addr(addr, protocol, ps_list);
	if (p == NULL){
		p = kmalloc(sizeof(struct ivi_ps), GFP_ATOMIC);
		p->addr.s_addr = addr;
		p->protocol = protocol;
		bitmap_zero(p->oldportbitmap, 65536);
		bitmap_zero(p->newportbitmap, 65536);
		INIT_LIST_HEAD(&p->portlist);
		spin_lock_init(&(p->addrlock));
		list_add(&(p->addrlist_entry), ps_list);
	}
	spin_unlock_irqrestore(lockp, flags);
	return ivi_ps_lookup_oldport(p, newport, ratio, offset);
}
EXPORT_SYMBOL(ivi_ps_getoldp);

module_init(init_ps);
module_exit(exit_ps);
