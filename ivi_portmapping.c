/*
 * =====================================================================================
 *
 *       Filename:  ivi_hgi4.c
 *
 *    Description:  used for ivi_hgi4, implement port mapping
 *
 *        Version:  1.0
 *        Created:  09/04/2009 09:21:18 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhai Yu (Jacky), jacky.zhai@gmail.com
 *        Company:  THU
 *
 * =====================================================================================
 */

#include <linux/module.h>
#include <linux/jiffies.h>
MODULE_AUTHOR("Hong Zhang & Zhai Yu");
MODULE_LICENSE("Dual BSD/GPL");

#define PSTART 8192
#define TIMEOUT 75000
spinlock_t pmlock;
static __u16 oldnew[65536];
static __u16 newold[65536];
static __u32 lasttime[65536];
static __u8 old_mapped[65536];
static __u8 new_mapped[65536];
int init_pm(void)
{
	int i;
	spin_lock_init(&pmlock);
	for ( i = 0; i < 65536; i++){
		oldnew[i] = 0;
	       	newold[i] = 0;
	       	lasttime[i] = 0;
	       	old_mapped[i] = 0;
	       	new_mapped[i] = 0;
	}
	return 0;
}
int ivi_getnewp ( __u16 oldp, __u16 mul, __u16 id )
{
	unsigned long flags;
	int p = PSTART + id;
	__u32 time;
	spin_lock_irqsave(&pmlock, flags);
	time = jiffies;
	if (old_mapped[oldp]){
	       if(time - lasttime[oldnew[oldp]] < TIMEOUT){
		       lasttime[oldnew[oldp]] = time;
		       p = oldnew[oldp];
		       goto out;
	       }
	       else{
		       old_mapped[oldp] = 0;
		       new_mapped[oldnew[oldp]] = 0;
	       }
	}
	for( p = PSTART + id; p < 65536; p += mul){
		if (new_mapped[p] == 0)
			break;
		if (time - lasttime[p] >= TIMEOUT){
			new_mapped[p] = 0;
			old_mapped[newold[p]] = 0;
			break;
		}
	}
	if (p >= 65536){
		p = -1;
		goto out;
	}
	oldnew[oldp] = p;
	newold[p] = oldp;
	old_mapped[oldp] = 1;
	new_mapped[p] = 1;
	lasttime[p] = time;
out:	spin_unlock_irqrestore(&pmlock, flags);
	return p;
}
EXPORT_SYMBOL(ivi_getnewp);
int ivi_getoldp (__u16 newp)
{
	__u32 time;
	unsigned long flags;
	int res;
	spin_lock_irqsave(&pmlock, flags);
	time = jiffies;
	if (new_mapped[newp] == 0){
		res = -1;
		goto out;
	}
	/* Even if under TIMEOUT circumstance, we refresh the timer, 
	 * this improves robustness 
	 */
	lasttime[newp] = time;
	res = newold[newp];
out:	spin_unlock_irqrestore(&pmlock, flags);
	return res;
}
EXPORT_SYMBOL(ivi_getoldp);
void exit_pm(void)
{
	printk("hgi4 removed\n");
}
module_init(init_pm);
module_exit(exit_pm);
