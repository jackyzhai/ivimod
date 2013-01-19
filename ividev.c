/* ividev.c
 * IVI Virtual Device Implementation
 * IVI Project, CERNET
 * create:                                2008.11.05  Hong Zhang
 */

#include "ivi.h"

MODULE_AUTHOR("Zhang Hong & Zhai Yu");
MODULE_LICENSE("Dual BSD/GPL");


/*
 * Transmitter lockup simulation, normally disabled.
 */
static int lockup = 0;
module_param(lockup, int, 0);

static int timeout = IVI_TIMEOUT;
module_param(timeout, int, 0);

/*
 * Do we run in NAPI mode?
 */
static int use_napi = 0;
module_param(use_napi, int, 0);


struct packet
{
	int datalen;
	u8 data[IVI_DATA_LEN];
};

struct ivi_priv {
	struct net_device_stats stats;
	int status;
	struct sk_buff *skb;
	spinlock_t lock;
};

/* for mapping entry */

struct mapping_entry4 * mib4[65536];
struct mapping_entry6 * mib6[65536];

static void ivi_tx_timeout(struct net_device *dev);

int ivi_open(struct net_device *dev)
{
	
	unsigned char mac[6];
	mac[0] = 0x00;
	mac[1] = 0x12;
	mac[2] = 0x34;
	mac[3] = 0x56;
	mac[4] = 0x78;
	mac[5] = 0x90;
	memcpy(dev->dev_addr, (void *)mac, ETH_ALEN);
	if (ivi_devs[0] != NULL && dev != ivi_devs[0])
		dev->dev_addr[ETH_ALEN-1]++;
	netif_start_queue(dev);
	return 0;
}

int ivi_release(struct net_device *dev)
{
	netif_stop_queue(dev); /* can't transmit any more */
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int ivi_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "ivi: Can't change I/O address\n");
		return -EOPNOTSUPP;
	}

	/* Allow changing the IRQ */
	if (map->irq != dev->irq) {
		dev->irq = map->irq;
        	/* request_irq() is delayed to open-time */
	}

	/* ignore other fields */
	return 0;
}

      
/* IVI mapping lookup
 * success : 1
 * fail:     0
 */
struct mapping_entry4 *  mapping_lookup4 ( __u32 index, __u32 lo )
{
	__u16 entry;
	
	if ( index == lo )
	{
		//printk(KERN_ALERT "SELF PACKET!");		
		return NULL;
	}
	//*sp = ntohl(0x20010da9);
	//*dp = ntohl(0x20010da9);
	entry = ntohl(index) & 0x0000ffff;
	return mib4[entry];	
}

struct mapping_entry6 * mapping_lookup6 ( struct in6_addr * add )
{
	__u16 entry;
	entry = ntohs(add->s6_addr16[7]);
	return mib6[entry];
}

int ivi_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct mapping_entry4 * map4, * new4, map4_cpy;
	struct mapping_entry6 * map6, * new6, map6_cpy;
	__u16 index;

	switch(cmd)
	{
		case IVI_IOC_ADD4:
		{
			map4 = (struct mapping_entry4 *)(rq->ifr_ifru.ifru_data);
			if(copy_from_user(&map4_cpy, map4, sizeof(struct mapping_entry4)))
				return -EFAULT;
			index = ntohl(map4_cpy.index.s_addr) & 0x0000ffff;
			if ( mib4[index] != NULL ){
				map4->error = IVI_IOC_ERR_EXIST;
				return -EFAULT;
			}
			new4 = kmalloc(sizeof(struct mapping_entry4), GFP_KERNEL);
			memcpy(new4, &map4_cpy, sizeof(struct mapping_entry4));
			printk(KERN_ALERT "IVI: Add IPv4 mapping entry No.%u\n", index);
			mib4[index] = new4;
			return 0;
		}
		case IVI_IOC_ADD6:/*Fix: use copy_from_user*/
		{
			map6 = (struct mapping_entry6 *)(rq->ifr_ifru.ifru_data);
			if(copy_from_user(&map6_cpy, map6, sizeof(struct mapping_entry6)))
				return -EFAULT;
			index = ntohs(map6_cpy.index.s6_addr16[7]);

			if ( mib6[index] != NULL ){
				map6->error = IVI_IOC_ERR_EXIST;
				return -EFAULT;
			}
			new6 = kmalloc(sizeof(struct mapping_entry6), GFP_KERNEL);
			memcpy(new6, &map6_cpy, sizeof(struct mapping_entry6));
			printk(KERN_ALERT "IVI: Add IPv6 mapping entry No.%u\n", index);
			mib6[index] = new6;
			return 0;
		}
		case IVI_IOC_DEL4:/*Fix: use copy_from_user*/
		{
			map4 = (struct mapping_entry4 *)(rq->ifr_ifru.ifru_data);
			index = ntohl(map4->index.s_addr) & 0x0000ffff;

			if ( mib4[index] == NULL )
			{
				map4->error = IVI_IOC_ERR_NOEXIST;
				return 1;
			}

			kfree(mib4[index]);
			mib4[index] = NULL;
				
			printk(KERN_ALERT "IVI: Del IPv4 mapping entry No.%u\n", index);
	
			return 0;
		}
		case IVI_IOC_DEL6:
		{
			map6 = (struct mapping_entry6 *)(rq->ifr_ifru.ifru_data);
			index = ntohs(map6->index.s6_addr16[7]);

			if ( mib6[index] == NULL )
			{
				map6->error = IVI_IOC_ERR_NOEXIST;
				return 1;
			}

			kfree(mib6[index]);
			mib6[index] = NULL;	
			
			printk(KERN_ALERT "IVI: Del IPv6 mapping entry No.%u\n", index);
	
			return 0;
		}
		default:
			return -EINVAL;
	}
}

/*
 * Transmit a packet (called by the kernel)
 */
int ivi_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	ssize_t newlen;
	unsigned char *data, shortpkt[ETH_ZLEN];
	struct ivi_priv *priv = netdev_priv(dev);
	struct net_device *dest;
	struct rtable *rt4;
	struct rt6_info * rt6;
	struct in6_addr * in6a;
	struct in6_addr * in6b;
	struct packet tx_buff;
	struct sk_buff *rcv_skb;

	
	struct mapping_entry4 * entry4 = NULL;
	struct mapping_entry6 * entry6 = NULL;
												
	data = skb->data;
	len = skb->len;
	newlen = len;
	if (len < ETH_ZLEN) {
		memset(shortpkt, 0, ETH_ZLEN);
		memcpy(shortpkt, skb->data, skb->len);
		len = ETH_ZLEN;
		data = shortpkt;
	}
	dev->trans_start = jiffies; /* save the timestamp */
	dest = ivi_devs[dev == ivi_devs[0] ? 1 : 0];
    	priv = netdev_priv(dest);
	
	/* IVI CORE */
	memset(&tx_buff, 0, sizeof(tx_buff));
	if (!skb->dst)		/* I dont know why*/
		goto out;
	switch(ntohs(skb->protocol)){
		case ETH_P_IP: 
			rt4 = (struct rtable *)(skb->dst);
			entry4 = mapping_lookup4(rt4->rt_gateway, rt4->rt_dst);
			if ( entry4 == NULL )
				goto out;
			newlen = ip_mapping(skb, tx_buff.data, entry4);
			if (newlen < 0){
				goto out;
			}
			break;
		case ETH_P_IPV6:
			rt6 = (struct rt6_info *)skb->dst;
			in6a = &(rt6->rt6i_gateway);
			in6b = &((rt6->rt6i_dst).addr);
			if ( !strncmp((char *)in6a, (char *)in6b, 16))  /*still don't know why*/
				goto out;
					//in6a->s6_addr32[0] != in6b->s6_addr32[0] || in6a->s6_addr32[1]!=in6b->s6_addr32[1] || in6a->s6_addr32[2]!=in6b->s6_addr32[2] || in6a->s6_addr32[3]!=in6b->s6_addr32[3])
			entry6 = mapping_lookup6(in6a);
			if ( entry6 == NULL ){
				printk("entry6 none\n");
				goto out;
			}
			newlen = ip6_mapping(skb, tx_buff.data, entry6);
			if (newlen < 0)
				goto out;
			break;
		default:
			goto out;
	}
	/* IVI DONE */
	spin_lock(&priv->lock);

	tx_buff.datalen = newlen;

	//RCV PROCESS
	rcv_skb = dev_alloc_skb(tx_buff.datalen + 2);
	if (!rcv_skb) 
	{
		if (printk_ratelimit())
			printk(KERN_NOTICE "ivi rx: low on mem - packet dropped\n");
		priv->stats.rx_dropped++;
		return 0;
	}
	skb_reserve(rcv_skb, 2); /* align IP on 16B boundary */  
	memcpy(skb_put(rcv_skb, tx_buff.datalen), tx_buff.data, tx_buff.datalen);

	rcv_skb->dev = dest;
	rcv_skb->protocol = eth_type_trans(rcv_skb, dest);
	rcv_skb->ip_summed = CHECKSUM_NONE;// CHECKSUM_UNNECESSARY;/* don't check it */
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += tx_buff.datalen;

	netif_rx(rcv_skb);


	//CLEAN AFTER TX
	spin_unlock(&priv->lock);
    	priv = netdev_priv(dev);
	spin_lock(&priv->lock);
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += len;
	spin_unlock(&priv->lock);

out:	priv = netdev_priv(dev);
	spin_lock(&priv->lock);
	dev_kfree_skb(skb);

	spin_unlock(&priv->lock);

	return 0; /* Our simple device can not fail */
}

/*
 * Deal with a transmit timeout.
 */
void ivi_tx_timeout (struct net_device *dev)
{
	struct ivi_priv *priv = netdev_priv(dev);
	priv->stats.tx_errors++;
	netif_wake_queue(dev);
	return;
}


/*
 * Return statistics to the caller
 */
struct net_device_stats *ivi_stats(struct net_device *dev)
{
	struct ivi_priv *priv = netdev_priv(dev);
	return &priv->stats;
}

/*
 * The "change_mtu" method is usually not needed.
 * If you need it, it must be like this.
 */
int ivi_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	struct ivi_priv *priv = netdev_priv(dev);
	spinlock_t *lock = &priv->lock;
    
	/* check ranges */
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;
	spin_lock_irqsave(lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(lock, flags);
	return 0; /* success */
}

/*
 * The init function (sometimes called probe).
 * It is invoked by register_netdev()
 */
void ivi_init(struct net_device *dev)
{
	struct ivi_priv *priv;
	ether_setup(dev); /* assign some of the fields */

	dev->open            = ivi_open;
	dev->stop            = ivi_release;
	dev->set_config      = ivi_config;
	dev->hard_start_xmit = ivi_tx;
	dev->do_ioctl        = ivi_ioctl;
	dev->get_stats       = ivi_stats;
	dev->change_mtu      = ivi_change_mtu;  
	//dev->set_mac_address = ivi_set_mac;
	dev->tx_timeout      = ivi_tx_timeout;
	dev->watchdog_timeo  = timeout;
	//if (use_napi) {
	//	dev->poll        = ivi_poll;
	//	dev->weight      = 2;
	//}
	/* keep the default flags, just add NOARP */
	dev->flags           |= IFF_NOARP;
//	dev->features        |= NETIF_F_NO_CSUM;
	//dev->hard_header_cache = NULL;      /* Disable caching */

	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct ivi_priv));
	spin_lock_init(&priv->lock);
	ivi_open(dev);
}

/*
 * The devices
 */

struct net_device *ivi_devs[2] = { NULL, NULL };



/*
 * Finally, the module stuff
 */

void ivi_cleanup(void)
{
	int i;
    
	for (i = 0; i < 2; i++)
	{
		if (ivi_devs[i])
		{
			unregister_netdev(ivi_devs[i]);
			free_netdev(ivi_devs[i]);
		}
	}
	for (i = 0; i < 65535; i++)
	{
		if ( mib4[i] != NULL )
			kfree(mib4[i]);
		if ( mib6[i] != NULL )
			kfree(mib6[i]);
	}
	return;
}




int ivi_init_module(void)
{
	int result, i, ret = -ENOMEM;

	/* Allocate the devices */
	ivi_devs[0] = alloc_netdev(sizeof(struct ivi_priv), "ivi%d",
			ivi_init);
	ivi_devs[1] = alloc_netdev(sizeof(struct ivi_priv), "ivi%d",
			ivi_init);
	//printk(KERN_ALERT "init01\n");
	if (ivi_devs[0] == NULL || ivi_devs[1] == NULL)
	{
		printk(KERN_ALERT "ivi error\n");
		goto out;
	}

	ret = -ENODEV;
	for (i = 0; i < 65535; i++)
	{
		mib4[i] = NULL;
		mib6[i] = NULL;
	}
	for (i = 0; i < 2; i++)
		if ((result = register_netdev(ivi_devs[i])))
			printk("ivi: error %i registering device \"%s\"\n",
					result, ivi_devs[i]->name);
		else
			ret = 0;
   out:
	if (ret) 
		ivi_cleanup();
	return ret;
}


module_init(ivi_init_module);
module_exit(ivi_cleanup);
