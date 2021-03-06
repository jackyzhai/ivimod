/* trans.c
 * Protocol Translation Implementation
 * IVI Project, CERNET
 * old version as proto_trans.c
 * create:                                2005.11.02  Hong Zhang
 * add:         Path MTU Discovery        2005.11.11  Hong Zhang
 * patch:       error when "TCP in ICMP"  2006.04.19  Hong Zhang
 * move to mod:                           2008.11.05  Hong Zhang
 */

#include "ivi.h"
#include "ivi_partialstate.h"
u16 rewrite_chksum4to6(__sum16 orig, struct iphdr *iph4)
{
	uint val = (~orig) & 0xffff;
	__be32 addr = iph4->saddr;
	val = val > addr ? val - addr : val - addr -1;
	addr = iph4->daddr;
	val = val > addr ? val - addr : val - addr -1;
	return val;
}
u16 rewrite_chksum6to4(uint orig, struct iphdr *iph4, struct ipv6hdr *iph6, struct ivi_buff *ivib)
{
	u32 val = (~orig) & 0xffff;
	int i;
	u16 tmp;
	val = val >= ivib->sport6 ? val - ivib->sport6 : val - ivib->sport6 - 1;
	val = val >= ivib->dport6 ? val - ivib->dport6 : val - ivib->dport6 - 1;
	for(i = 0; i < 8 ; i++){
		tmp = iph6->saddr.s6_addr16[i];
		val = val >= tmp ? val - tmp : val - tmp - 1;
	}
	for(i = 0; i < 8 ; i++){
		tmp = iph6->daddr.s6_addr16[i];
		val = val >= tmp ? val - tmp : val  - tmp - 1;
	}
	return csum_tcpudp_magic(iph4->saddr, iph4->daddr, ntohs(ivib->sport4), ntohs(ivib->dport4), val);
}
/*return the length of the new ipv6 header(may include the fragment header*/
ssize_t iphdr4to6(struct ivi_buff *ivib)
{
	int i;
	__u8 a, b;
	__u16 *sport, *dportp, dport, portcoding, ratio, offset;
	struct iphdr *iphdr4;
	struct ipv6hdr *iphdr6;
	struct mapping_entry4 *entry = ivib->entry4;
	struct icmphdr *icmph;
	__be32 tmpaddr4;
	int port;
	int pingbeacon6 = 0;
	int pingbeacon4 = 0;
	if (ivib->embeded){
		iphdr6 = ivib->iphdr6_embed;
		iphdr4 = ivib->iphdr4_embed;
	}
	else{
		iphdr6 = ivib->iphdr6;
		iphdr4 = ivib->iphdr4;
	}
	if(iphdr4->ihl != 5){
		printk(KERN_INFO "IP HEADER LEN %d \n", (int)iphdr4->ihl*4);
		return -1;
	}
	if((ntohs(iphdr4->frag_off) & 0x1FFF) != 0)
		return -1;
	iphdr6->priority = 0;
	iphdr6->version = 6;
	iphdr6->flow_lbl[0] = 0;
	iphdr6->flow_lbl[1] = 0;
	iphdr6->flow_lbl[2] = 0;
	iphdr6->nexthdr = iphdr4->protocol;
	if (iphdr4->protocol == IPPROTO_ICMP) 
		iphdr6->nexthdr = IPPROTO_ICMPV6;
        iphdr6->hop_limit = iphdr4->ttl;
       	iphdr6->payload_len = htons(ntohs(iphdr4->tot_len) - iphdr4->ihl * 4);

	for (i = 0; i < 4; i++){
		iphdr6->saddr.s6_addr32[i] = 0;
		iphdr6->daddr.s6_addr32[i] = 0;
	}

/*src addr and src port manipulation*/	
	iphdr6->saddr = entry->srcp;

	if (entry->mode & IVI_4HOST_MULTIPLEX){
		a = entry->srcl/32;
		b = entry->srcl%32;
		iphdr6->saddr.s6_addr32[a] |= iphdr4->saddr << b;
		if (b)
			iphdr6->saddr.s6_addr32[a+1] |= iphdr4->saddr >> (32-b);
		if (iphdr4->protocol == IPPROTO_TCP || iphdr4->protocol == IPPROTO_UDP) /* currently don't care icmp in 1:N */
			sport=&(((struct tcphdr *)((void *)iphdr4 + 20))->source);
		else if (iphdr4->protocol == IPPROTO_ICMP){
			icmph = (struct icmphdr *)((void *)iphdr4 + 20);
			if (icmph->type == ICMP_ECHO || icmph->type == ICMP_ECHOREPLY)
				sport = &(icmph->un.echo.id);
			else
				return -1;
			if (icmph->type == ICMP_ECHOREPLY && entry->mode & IVI_PING_BEACON4)
				pingbeacon4 = 1;
		}
		else
			return -1;
		if (entry->mode & IVI_HGI4){
			portcoding = (entry->src_ratio) << 12;
			portcoding |= entry->hgi4_offset;
			ratio = 0x01 << (entry->src_ratio);
			if (!ratio)
				return -1;
			a = entry->srcl/16 + 2;
			b = entry->srcl%16;
			iphdr6->saddr.s6_addr16[a] |= htons(portcoding) << b;
			if (b)
				iphdr6->saddr.s6_addr16[a + 1] |= htons(portcoding) >> (16 - b);
		/*	iphdr6->saddr.s6_addr16[6] = htons(entry->src_ratio);
		 *	iphdr6->saddr.s6_addr16[7] = htons(entry->hgi4_offset);
		*/
			/*record port4 & port 6, for recalculating checksum
			 * more beautiful approach to do this might be write port mapping in
			 * upper layer translation like tcp4to6, udp4to6, icmp4to6
			 */
			ivib->sport4 = *sport;
			if (!pingbeacon4){
				port = ivi_ps_getnewp(ntohs(*sport),iphdr4->saddr, ratio, entry->hgi4_offset, iphdr4->protocol, 4);
				if (port < 0)
					return -1;
				*sport = htons(port);
			}
			ivib->sport6 = *sport;
		}
		else{
			portcoding = (entry->src_ratio) << 12;
			ratio = 0x01 << (entry->src_ratio);
			portcoding |= ntohs(*sport)%ratio;
			a = entry->srcl/16 + 2;
			b = entry->srcl%16;
			iphdr6->saddr.s6_addr16[a] |= htons(portcoding) << b;
			if (b)
				iphdr6->saddr.s6_addr16[a + 1] |= htons(portcoding) >> (16 - b);
			/*iphdr6->saddr.s6_addr16[6] = htons(entry->src_ratio);
			 *iphdr6->saddr.s6_addr16[7] = htons(ntohs(*sport)%entry->src_ratio);
			 */
		}
	}
	else if(entry->mode & IVI_PS4){
		tmpaddr4 = entry->hgi_prefix;
		((u8 *)(&tmpaddr4))[3] = ((u8 *)(&(iphdr4->saddr)))[1];
		a = entry->srcl/32;
		b = entry->srcl%32;
		iphdr6->saddr.s6_addr32[a] |= tmpaddr4 << b;
		if (b)
			iphdr6->saddr.s6_addr32[a+1] |= tmpaddr4 >> (32-b);
		if (iphdr4->protocol == IPPROTO_TCP || iphdr4->protocol == IPPROTO_UDP) /* currently don't care icmp in 1:N */
			sport=&(((struct tcphdr *)((void *)iphdr4 + 20))->source);
		else if (iphdr4->protocol == IPPROTO_ICMP){
			icmph = (struct icmphdr *)((void *)iphdr4 + 20);
			if (icmph->type == ICMP_ECHO || icmph->type == ICMP_ECHOREPLY)
				sport = &(icmph->un.echo.id);
			else
				return -1;
			if (icmph->type == ICMP_ECHOREPLY && entry->mode & IVI_PING_BEACON4)
				pingbeacon4 = 1;
		}
		else
			return -1;
		portcoding = ((u16 *)(&(iphdr4->saddr)))[1];
		ratio = 0x01 << (ntohs(portcoding) >> 12);
		offset = ntohs(portcoding) & 0xfff;
		a = entry->srcl/16 + 2;
		b = entry->srcl%16;
		iphdr6->saddr.s6_addr16[a] |= portcoding << b;
		if (b)
			iphdr6->saddr.s6_addr16[a + 1] |= portcoding >> (16 - b);
		/*record port4 & port 6, for recalculating checksum
		* more beautiful approach to do this might be write port mapping in
		* upper layer translation like tcp4to6, udp4to6, icmp4to6
		*/
		ivib->sport4 = *sport;
		if (!pingbeacon4){
			port = ivi_ps_getnewp(ntohs(*sport),tmpaddr4, ratio, offset, iphdr4->protocol, 4);
			if (port < 0)
				return -1;
			*sport = htons(port);
		}
		ivib->sport6 = *sport;

	}
	else{
		a = entry->srcl/32;
		b = entry->srcl%32;
		iphdr6->saddr.s6_addr32[a] |= iphdr4->saddr << b;
		if (b)
			iphdr6->saddr.s6_addr32[a+1] |= iphdr4->saddr >> (32-b);
	}

	/*dst addr and dst port manipulation*/
	if(entry->mode & IVI_6HOST_STATEFUL){
		__u32 id;
        	id = ntohl(iphdr4->daddr) & 0x0000FFFF;
		if(ivi_stateful_get6(id, &(iphdr6->daddr)) < 0)
			return -1;;
	}
	else{
		iphdr6->daddr = entry->dstp;
        	a = entry->dstl/32;
		b = entry->dstl%32;
		iphdr6->daddr.s6_addr32[a] |= iphdr4->daddr << b;
		if(b)
			iphdr6->daddr.s6_addr32[a+1] |= iphdr4->daddr >> (32 - b);
		if(entry->mode & IVI_6HOST_MULTIPLEX){
			if (iphdr4->protocol == IPPROTO_TCP || iphdr4->protocol == IPPROTO_UDP){
				dportp = &(((struct tcphdr *)((void *)iphdr4 + 20))->dest);
			}
			else if (iphdr4->protocol == IPPROTO_ICMP){
				icmph = (struct icmphdr *)((void *)iphdr4 + 20);
				if (icmph->type == ICMP_ECHO){
					if (entry->mode & IVI_PING_BEACON6)
						pingbeacon6 = 1;
				       dportp = &(icmph->un.echo.id);
				}
				else if (icmph->type == ICMP_ECHOREPLY)
				       dportp = &(icmph->un.echo.id);
				else
					return -1;
			}
			else
				return -1;
			dport = ntohs(*dportp);
			ratio = 0x01 << (entry->dst_ratio);
			offset = dport%ratio;
			ivib->dport4 = *dportp;
			if(entry->mode & IVI_PS6 && !pingbeacon6){
				port = ivi_ps_getoldp(dport,iphdr4->daddr, ratio, offset, iphdr4->protocol, 6);
				if (port < 0)
					return -1;
				*dportp = htons(port);
			}
			ivib->dport6 = *dportp;
			portcoding = (entry->dst_ratio) << 12;
			if (pingbeacon6)
				portcoding |= 0;
			else
				portcoding |= offset;
			a = entry->dstl/16 + 2;
			b = entry->dstl%16;
			iphdr6->daddr.s6_addr16[a] |= htons(portcoding) << b;
			if(b)
				iphdr6->daddr.s6_addr16[a + 1] |= htons(portcoding) >> (16 - b);
			}
    	}
	ivib->ptr4 += 20;
	ivib->ptr6 += 40;	
	return 40;	
}
/*Translate ICMP to ICMPv6*/
ssize_t icmp4to6(struct ivi_buff *ivib)
{
	struct icmphdr * icmph4 = (struct icmphdr *) ivib->ptr4;
	struct icmp6hdr * icmph6 = (struct icmp6hdr *) ivib->ptr6;
	struct ipv6hdr *iph6;
	size_t len;
	__be32 icmp_un_data32 = icmph4->un.gateway;
	unsigned short embeded = 0;
	if(ivib->embeded)
		iph6 = ivib->iphdr6_embed;
	else
		iph6 = ivib->iphdr6;
	switch(icmph4->type){
		case ICMP_ECHOREPLY:
			icmph6->icmp6_type = ICMPV6_ECHO_REPLY;
			icmph6->icmp6_code = 0;
			embeded = 0;
			break;
		case ICMP_ECHO:
			icmph6->icmp6_type = ICMPV6_ECHO_REQUEST;
			icmph6->icmp6_code = 0;
			embeded = 0;
			break;
		case ICMP_DEST_UNREACH:
			embeded = 1;
			icmph6->icmp6_type = ICMPV6_DEST_UNREACH;
			switch(icmph4->code){
				case ICMP_NET_UNREACH:	/* Network Unreachable		*/
					icmph6->icmp6_code = ICMPV6_NOROUTE;
					break;
				case ICMP_PORT_UNREACH:	/* Port Unreachable		*/
					icmph6->icmp6_code = ICMPV6_PORT_UNREACH;
					break;
				case ICMP_FRAG_NEEDED:	/* Fragmentation Needed/DF set	*/
					icmph6->icmp6_type = ICMPV6_PKT_TOOBIG;
					icmph6->icmp6_code = 0;
					icmp_un_data32 = htonl(ntohl(icmph4->un.frag.mtu) + 20);
					break;
				case ICMP_PKT_FILTERED:	/* Packet filtered */
					icmph6->icmp6_code = ICMPV6_ADM_PROHIBITED;
					break;
				case ICMP_HOST_UNREACH:	/* Host Unreachable		*/
				case ICMP_PROT_UNREACH:	/* Protocol Unreachable		*/
				default:
					icmph6->icmp6_code = ICMPV6_ADDR_UNREACH;
					break;
			}
			break;
		case ICMP_TIME_EXCEEDED:
			icmph6->icmp6_type = ICMPV6_TIME_EXCEED;
			icmph6->icmp6_code = icmph4->code;
			embeded = 0;
			break;
		default:
			return -1;
	}
	icmph6->icmp6_dataun.un_data32[0] = icmp_un_data32;
	ivib->ptr6 += sizeof(struct icmp6hdr);
	ivib->ptr4 += sizeof(struct icmphdr);
	switch(embeded){
		case 0:
			len = ivib->end4 - ivib->ptr4;
			memcpy(ivib->ptr6, ivib->ptr4, len);
			ivib->ptr4 += len;
			ivib->ptr6 += len;
			break;
		case 1:
			if(ivib->embeded == 1){
				printk("IVI:ICMP ERR message in ICMP ERR message, we don't expect this happen\n");
				return -1;
			}
			ivib->iphdr4_embed = (struct iphdr *)ivib->ptr4;
			ivib->iphdr6_embed = (struct ipv6hdr *)ivib->ptr6;
			ivib->embeded = 1;
			if(ip4to6(ivib) < 0)
				return -1;
			break;
		default:
			return -1;
	}
	len = ivib->ptr6 - (__u8 *)icmph6;
	icmph6->icmp6_cksum = 0;
	icmph6->icmp6_cksum = csum_ipv6_magic(&(iph6->saddr), &(iph6->daddr), len,\
		       	IPPROTO_ICMPV6, csum_partial(icmph6, len, 0));
	return 0;
}

/*Translate TCP on IPv4 to TCP on IPv6
 * on succest, return 0
 * error, return -1*/

ssize_t tcp4to6(struct ivi_buff *ivib)
{
	struct tcphdr * tcph6 = (struct tcphdr *) ivib->ptr6;
	struct tcphdr * tcph4 = (struct tcphdr *) ivib->ptr4;
	__u16 * tcpdata = (__u16 *)tcph6;
	size_t len = ivib->end4 - ivib->ptr4;
	struct ipv6hdr *iph6;
	struct in6_addr *saddr;
	struct in6_addr *daddr;
	memcpy(tcph6, tcph4, len);

	if(ivib->embeded == 1) {   /*for embeded ip packet, simply memcpy it would be ok*/
		iph6 = ivib->iphdr6_embed;
		if(ntohs(iph6->payload_len) != len)
			goto out;
	}
	else
		iph6 = ivib->iphdr6;
	saddr = (struct in6_addr *)(&(iph6->saddr));
	daddr = (struct in6_addr *)(&(iph6->daddr)); 	
	if ((ivib->entry4->mode & IVI_MSS_CLAMPING) && tcph6->syn && (tcph6->doff > 5)){
		if(ntohs(tcpdata[10])==0x0204)
			tcpdata[11]=htons(ntohs(tcpdata[11])-20);
	}
	tcph6->check = 0;
	tcph6->check = csum_ipv6_magic(saddr, daddr, len, IPPROTO_TCP, csum_partial(tcph6,len,0));
out:	ivib->ptr6 += len;
	ivib->ptr4 += len;
	return 0;
}

/*Translate UDP on IPv4 to UDP on IPv6*/

ssize_t udp4to6(struct ivi_buff *ivib)
{
	struct udphdr * udph6 = (struct udphdr *)ivib->ptr6;
	size_t len = ivib->end4 - ivib->ptr4;
	struct ipv6hdr *iph6;
	memcpy(udph6, ivib->ptr4 ,len);
	if(ivib->embeded == 1){
	       iph6 = ivib->iphdr6_embed;
	       if(ntohs(iph6->payload_len) != len)
		       goto out;
	}
	else
		iph6 = ivib->iphdr6;
	udph6->check = 0;
	udph6->check = csum_ipv6_magic((struct in6_addr *)(&(iph6->saddr)), (struct in6_addr *)(&(iph6->daddr)),\
		       	len, IPPROTO_UDP, csum_partial(udph6, len, 0));
out:	ivib->ptr4 += len;
	ivib->ptr6 += len;
	return 0;
}

ssize_t iphdr6to4(struct ivi_buff *ivib)
{
	__u16 ratio, offset, portcoding;
	__u16 * dport;
	__u16 * sportp;
	__u8 a, b, *tmp8p;
	__u32 tmp;
	int port, tot_len;
	struct ipv6hdr *iph6;
	struct iphdr *iph4;
	struct mapping_entry6 *entry = ivib->entry6;
	struct icmp6hdr *icmph6;
	int hdrlen6 = 40;  /*ipv6 hdrlen, including fragment hdr*/
	int pingbeacon4 = 0;
	int pingbeacon6 = 0;
	struct frag_hdr *frag;

	if (ivib->embeded){
		iph6 = ivib->iphdr6_embed;
		iph4 = ivib->iphdr4_embed;
	}
	else{
		iph6 = ivib->iphdr6;
		iph4 = ivib->iphdr4;
	}
	iph4->version  = 4;
	iph4->ihl      = 5;
	iph4->tos      = 0;   /* Normal TOS */
	iph4->ttl = iph6->hop_limit;
	iph4->check = 0;
	tot_len = ntohs(iph6->payload_len) + 20;
	/*about fragment*/
	if (iph6->nexthdr == NEXTHDR_FRAGMENT) {
		hdrlen6 += 8;
		tot_len -= 8;
		ivib->fragmented = 1;
		frag = (struct frag_hdr *)((void *)iph6 + 40);
		iph4->protocol = frag->nexthdr;
		iph4->frag_off = htons(ntohs(frag->frag_off) >> 3);  
		if (ntohs(frag->frag_off) & 0x0001) /*MF bit*/
			iph4->frag_off |= htons(0x2000);
		if(ntohs(iph6->payload_len) > 1280)
			iph4->frag_off |= htons(0x4000);  /*set DF anyway with pack size > 1280*/
		iph4->id = htons(ntohl(frag->identification));
	}
	else{
		iph4->protocol = iph6->nexthdr;
		iph4->id = 0;
		iph4->frag_off = htons(0x4000);
	}

	/*for icmp error message, the "out-layer" ip hdr's tot_len would be rewrite in ip6to4*/
	iph4->tot_len = htons(tot_len);

	/*end fragment*/
	if (iph4->protocol == IPPROTO_ICMPV6)
		iph4->protocol = IPPROTO_ICMP;

	/*we don't deal with ipv6 extention header except fragment header*/
	if (iph4->protocol != IPPROTO_ICMP && iph4->protocol != IPPROTO_TCP && iph4->protocol != IPPROTO_UDP)
		return -1;
#ifdef	MAPPING_FRAGMENTATION
	*((__u32 *)(&(iph4->id))) = getfragmentation(iph6->saddr);
#endif

/*src addr and port translation*/
	if(entry->mode & IVI_6HOST_STATEFUL){
		tmp = ivi_stateful_get4(&(iph6->saddr));
		if(!tmp)
			return -1;
		iph4->saddr = htonl((0x0aff << 16) | tmp);
	}
	else if((entry->mode & IVI_TRACERT_ADDR_TRANS) && ((struct icmp6hdr *)((void *)iph6 + hdrlen6))->icmp6_type == ICMPV6_TIME_EXCEED){
		iph4->saddr = htonl((ntohl(entry->icmp_ttl_prefix) & 0xFFFFFF00) + iph4->ttl);
	}
	else{
		a = entry->srcl/32;
		b = entry->srcl%32;
		iph4->saddr = iph6->saddr.s6_addr32[a] >> b;
		if (b)
			iph4->saddr |= iph6->saddr.s6_addr32[a+1] << (32-b);
		a = entry->srcl/16 + 2;
		b = entry->srcl%16;
		portcoding = (iph6->saddr.s6_addr16[a] >> b) | (iph6->saddr.s6_addr16[a + 1] << (16 - b));
		portcoding = ntohs(portcoding);
		ratio = 0x01 << (portcoding >> 12);
		offset = portcoding & 0x0fff;
		if (ratio){
			if (iph4->protocol == IPPROTO_TCP || iph4->protocol == IPPROTO_UDP)
				sportp = &((struct tcphdr *)((void *)iph6 + hdrlen6))->source;
			else if (iph4->protocol == IPPROTO_ICMP){
				icmph6 = (struct icmp6hdr *)((void *)iph6 + hdrlen6);
				if (icmph6->icmp6_type == ICMPV6_ECHO_REPLY){
				       if (entry->mode & IVI_PING_BEACON6 )
					       pingbeacon6 = 1;
					       sportp = &icmph6->icmp6_dataun.u_echo.identifier;
				}
				else if (icmph6->icmp6_type == ICMPV6_ECHO_REQUEST)
					sportp = &icmph6->icmp6_dataun.u_echo.identifier;
				else
					return -1;
			}
			else
				return -1;
			if(!pingbeacon6 && entry->mode & IVI_PS6){
				ivib->sport6 = *sportp;
				port = ivi_ps_getnewp(ntohs(*sportp),iph4->saddr, ratio, offset, iph4->protocol, 6);
				if (port < 0)
					return -1;
				*sportp = htons(port);
				ivib->sport4 = *sportp;
			}
			if (!pingbeacon6 && ntohs(*sportp)%ratio != offset)
				return -1;
		}
	}
	/*dst addr and port translation*/
	a = entry->dstl/32;
	b = entry->dstl%32;
	iph4->daddr = iph6->daddr.s6_addr32[a] >> b;
	if (b)
		iph4->daddr |= iph6->daddr.s6_addr32[a+1] << (32-b);

	a = entry->dstl/16 + 2;
	b = entry->dstl%16;
	portcoding = (iph6->daddr.s6_addr16[a] >> b) | (iph6->daddr.s6_addr16[a + 1] << (16 - b));
	portcoding = ntohs(portcoding);
	ratio = 0x01 << (portcoding >> 12);
        offset = portcoding & 0xfff;
	if (ratio){
		if (iph4->protocol == IPPROTO_TCP || iph4->protocol == IPPROTO_UDP) 
			dport = &(((struct tcphdr *)((void *)iph6 + hdrlen6))->dest);
		else if (iph4->protocol == IPPROTO_ICMP){
			icmph6 = (struct icmp6hdr *)((void *)iph6 + hdrlen6);
			if (icmph6->icmp6_type == ICMPV6_ECHO_REPLY || icmph6->icmp6_type == ICMPV6_ECHO_REQUEST)
				dport = &(icmph6->icmp6_dataun.u_echo.identifier);
			else
				return -1;
			if (icmph6->icmp6_type == ICMPV6_ECHO_REQUEST && entry->mode & IVI_PING_BEACON4)
				pingbeacon4 = 1;
		}
		else
			return -1;
		if (!pingbeacon4 && ntohs(*dport)%ratio != offset)
			return -1;
		ivib->dport6 = *dport;
		if (!pingbeacon4){
			if (entry->mode & IVI_HGI4) {
				port = ivi_ps_getoldp(ntohs(*dport),iph4->daddr, ratio, offset, iph4->protocol, 4);
				if (port > 0)
					*dport = htons(port);
			}
			else if (entry->mode & IVI_PS4){
				port = ivi_ps_getoldp(ntohs(*dport),iph4->daddr, ratio, offset, iph4->protocol, 4);
				if (port > 0)
					*dport = htons(port);
			}
		}
		ivib->dport4 = *dport;
		if( entry->mode & IVI_PS4){
			tmp8p = (__u8 *)(&(iph4->daddr));
			tmp8p[0] = 10;
			tmp8p[1] = tmp8p[3];
			*((__u16 *)(tmp8p + 2)) = htons(portcoding);
		}
	}
	iph4->check = ip_fast_csum((unsigned char *)iph4, iph4->ihl);
	ivib->ptr4 += 20;
	ivib->ptr6 += hdrlen6;
	return 0;
}



/*Translate ICMPv6 to ICMP*/

int icmp6to4(struct ivi_buff *ivib)
{
	struct icmphdr * icmph4 = (struct icmphdr *) ivib->ptr4;
	struct icmp6hdr * icmph6 = (struct icmp6hdr *) ivib->ptr6;
	struct iphdr *iph4;
	size_t len;
	__be32 icmp6_un_data32 = icmph6->icmp6_dataun.un_data32[0];
	unsigned short embeded = 0;
	if(ivib->embeded)
		iph4 = ivib->iphdr4_embed;
	else
		iph4 = ivib->iphdr4;
	switch(icmph6->icmp6_type){
		case ICMPV6_ECHO_REPLY:
			icmph4->type = ICMP_ECHOREPLY;
			icmph4->code = 0;
			embeded = 0;
			break;
		case ICMPV6_ECHO_REQUEST:
			icmph4->type = ICMP_ECHO;
			icmph4->code = 0;
			embeded = 0;
			break;
		case ICMPV6_TIME_EXCEED:
			icmph4->type = ICMP_TIME_EXCEEDED;
			icmph4->code = icmph6->icmp6_code;
			embeded = 1;
			break;
		case ICMPV6_DEST_UNREACH:
			embeded = 1;
			icmph4->type = ICMP_DEST_UNREACH;
			switch (icmph6->icmp6_code){
				case ICMPV6_NOROUTE:
					icmph4->code = ICMP_NET_UNREACH;
					break;
				case ICMPV6_PORT_UNREACH:
					icmph4->code = ICMP_PORT_UNREACH;
					break;
				case ICMPV6_ADM_PROHIBITED:
					icmph4->code = ICMP_PKT_FILTERED;
					break;
				case ICMPV6_ADDR_UNREACH:
				default:
					icmph4->code = ICMP_HOST_UNREACH;
					break;
			}
			break;
		case ICMPV6_PKT_TOOBIG:
			embeded = 1;
			icmph4->type = ICMP_DEST_UNREACH;
			icmph4->code = ICMP_FRAG_NEEDED;
			icmp6_un_data32 = htonl(ntohl(icmph6->icmp6_mtu) - 28);
		default:
			return -1;
	}
	icmph4->un.gateway = icmp6_un_data32;
	ivib->ptr6 += sizeof(struct icmp6hdr);
	ivib->ptr4 += sizeof(struct icmphdr);
	switch(embeded){
		case 0:
			len = ivib->end6 - ivib->ptr6;
			memcpy(ivib->ptr4, ivib->ptr6, len);
			ivib->ptr4 += len;
			ivib->ptr6 += len;
			break;
		case 1:
			if(ivib->embeded == 1){
				printk("IVI:ICMPv6 ERR message in ICMPv6 ERR message, we don't expect this happen\n");
				return -1;
			}
			ivib->iphdr4_embed = (struct iphdr *)ivib->ptr4;
			ivib->iphdr6_embed = (struct ipv6hdr *)ivib->ptr6;
			ivib->embeded = 1;
			if(ip6to4(ivib) < 0)
				return -1;
			break;
		default:
			return -1;
	}
	len = ivib->ptr4 - (__u8 *)icmph4;
	icmph4->checksum = 0;
	icmph4->checksum = ip_compute_csum(icmph4, len);
	return 0;
}

/*Translate TCP on IPv6 to TCP on IPv4*/

ssize_t tcp6to4(struct ivi_buff *ivib)
{
	struct tcphdr * tcph6 = (struct tcphdr *) ivib->ptr6;
	struct tcphdr * tcph4 = (struct tcphdr *) ivib->ptr4;
	size_t len = ivib->end6 - ivib->ptr6;
	struct iphdr *iph4;
	struct ipv6hdr *iph6;
//	__be32 saddr, daddr;
	memcpy(tcph4, tcph6, len);

	if(ivib->embeded){
		iph4 = ivib->iphdr4_embed;
		iph6 = ivib->iphdr6_embed;
		if(ntohs(iph4->tot_len) - 20 != len)
		       goto out;
	}
	else{
		iph4 = ivib->iphdr4;
		iph6 = ivib->iphdr6;
	}
//	saddr = iph4->saddr;
//	daddr = iph4->daddr;
	tcph4->check = rewrite_chksum6to4(tcph6->check, iph4, iph6, ivib);
	
//	tcph4->check = 0;
//	tcph4->check = csum_tcpudp_magic(saddr, daddr, len, IPPROTO_TCP, csum_partial(tcph4,len,0));
out:	ivib->ptr6 += len;
	ivib->ptr4 += len;
	return 0;
}

/*Translate UDP on IPv6 to UDP on IPv4*/

ssize_t udp6to4(struct ivi_buff *ivib)
{
	struct udphdr * udph4 = (struct udphdr *)ivib->ptr4;
	size_t len = ivib->end6 - ivib->ptr6;
	struct iphdr *iph4;
	struct ipv6hdr *iph6;
	memcpy(udph4, ivib->ptr6 ,len);
	if(ivib->embeded == 1){
	       iph4 = ivib->iphdr4_embed;
	       iph6 = ivib->iphdr6_embed;
	       if(ntohs(iph4->tot_len) - 20 != len)
		       goto out;
	}
	else{
		iph4 = ivib->iphdr4;
		iph6 = ivib->iphdr6;
	}
	udph4->check = rewrite_chksum6to4(udph4->check, iph4, iph6, ivib);
//	udph4->check = 0;
//	udph4->check = csum_tcpudp_magic(iph4->saddr, iph4->daddr, len, IPPROTO_UDP, csum_partial(udph4,len,0));
out:
	ivib->ptr4 += len;
	ivib->ptr6 += len;
	return 0;
}
