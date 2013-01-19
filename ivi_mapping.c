/* mapping.c
 * IP and IPv6 Mapping Implementation
 * IVI Project, CERNET
 * create:                                2005.11.02  Ang Li
 * move to mod:                           2008.11.05  Hong Zhang
 * rewrite and change structure		  2009.9      Zhai Yu
 */

#include "ivi.h"

__u32 getfragmentation(const struct in6_addr oldv6) {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	return oldv6.s6_addr32[2];
#elif defined(__BIG_ENDIAN_BITFIELD)
	return oldv6.s6_addr32[1];
#else
#error 	"Please fix <asm/byteorder.h>"
#endif
}

/*on success, return 0
 * error, return -1
 * future: return payload length
 */
ssize_t ip4to6(struct ivi_buff *ivib)
{
	unsigned short nexthdr;
	int embeded = ivib->embeded;  /*preserve the value of ivib->embed*/
	ssize_t len = iphdr4to6(ivib);
	if (len	< 0 )    /* for fragment, we don't need a translation of \
							   trans-layer header & icmp header */
		return -1;       /*currently drop the fragments which is not the 1st fragment*/
	if (ivib->embeded){
		nexthdr = ivib->iphdr6_embed->nexthdr;
	}
	else
		nexthdr = ivib->iphdr6->nexthdr;
	switch (nexthdr){
        case IPPROTO_ICMPV6:
		if(icmp4to6(ivib) < 0)
			return -1;
            break;
        case IPPROTO_TCP:
	    if(tcp4to6(ivib) < 0)
		    return -1;
            break;
        case IPPROTO_UDP:
	    if(udp4to6(ivib) < 0)
		    return -1;
            break;
        default:
	    return -1;
	}
	if(!embeded && ivib->embeded)  /*the packet has an embeded ip packet*/
		ivib->iphdr6->payload_len = htons(ivib->ptr6 - (__u8 *)ivib->iphdr6 - len);
	return 0;
}
ssize_t ip6to4(struct ivi_buff *ivib)
{
	unsigned short protocol;
	int embeded = ivib->embeded;
	ssize_t len = iphdr6to4(ivib);
	struct iphdr *iph4;
	if(len < 0)
		return -1;
	if (ivib->embeded)
		iph4 = ivib->iphdr4_embed;
	else
		iph4 = ivib->iphdr4;
	/*if the packet was but not the 1st fragment*/
	if((ntohs(iph4->frag_off) & 0x1fff) != 0){
		memcpy(ivib->ptr4, ivib->ptr6, ivib->end6 - ivib->ptr6);
		goto out;
	}
	protocol = iph4->protocol;
	switch(protocol)
	{
		case IPPROTO_ICMP:
			if(icmp6to4(ivib) < 0)
				return -1;
			break;
		case IPPROTO_TCP:
			if(tcp6to4(ivib) < 0)
				return -1;
			break;
		case IPPROTO_UDP:
			if(udp6to4(ivib) < 0)
				return -1;
			break;
		default:
			return -1;
	}
out:	if(!embeded && ivib->embeded){
		iph4->tot_len = htons(ivib->ptr4 - (__u8 *)iph4);
		iph4->check = 0;
		iph4->check = ip_fast_csum((unsigned char *)iph4, iph4->ihl);
	}
	return 0;
}
ssize_t ip_mapping(struct sk_buff *skb, __u8 *newdata, struct mapping_entry4 * entry)
{ 
	struct ethhdr *neweth;
	struct ivi_buff ivib;
	memset(&ivib, 0, sizeof(ivib));
	ivib.entry4 = entry;
	/*eth header*/
	memcpy(newdata, skb->data, ETH_HLEN);
	neweth = (struct ethhdr*) newdata;
	neweth->h_proto = htons(ETH_P_IPV6);
	neweth->h_dest[5] = 0x91;

	ivib.iphdr4 = (struct iphdr *)skb_pull(skb, ETH_HLEN);
	ivib.ptr4 = (__u8 *)ivib.iphdr4;
	ivib.iphdr6 = (struct ipv6hdr *)(newdata + ETH_HLEN);
	ivib.ptr6 = (__u8 *)ivib.iphdr6;
	ivib.end4 = skb_tail_pointer(skb);
	if(ip4to6(&ivib) < 0){
		skb_push(skb, ETH_HLEN);
		return -1;       /*currently drop the fragments which is not the 1st fragment*/
	}
	skb_push(skb, ETH_HLEN);
	return ntohs(ivib.iphdr6->payload_len) + 40 + ETH_HLEN;
}

int ip6_mapping(struct sk_buff *skb, __u8 *newdata, struct mapping_entry6 * entry)
{
	struct ethhdr *neweth;
	struct ivi_buff ivib;
	memset(&ivib, 0, sizeof(ivib));

	memcpy(newdata, skb->data, ETH_HLEN);
	
	neweth = (struct ethhdr *) newdata;
    	neweth->h_proto = htons(ETH_P_IP);
	neweth->h_dest[5] = 0x90;

	ivib.entry6 = entry;
    	ivib.iphdr6 = (struct ipv6hdr *)skb_pull(skb, ETH_HLEN);
	ivib.ptr6 = (__u8 *)ivib.iphdr6;
	ivib.iphdr4 = (struct iphdr *)(newdata + ETH_HLEN);
	ivib.ptr4 = (__u8 *)ivib.iphdr4;
	ivib.end6 = skb_tail_pointer(skb);
	if(ip6to4(&ivib) < 0){
		skb_push(skb, ETH_HLEN);
		return -1;
	}
	skb_push(skb, ETH_HLEN);
	return ntohs(ivib.iphdr4->tot_len) + ETH_HLEN;
}
