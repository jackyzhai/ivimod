/* Command Line Interface for IVI
 * 2009-9-17 ZhaiYu
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <string.h>
#include "ivimap.h"
static struct option const longopts[]=
{
	{"srcprefix", required_argument, NULL, 'p'},
	{"dstprefix", required_argument, NULL, 'P'},
	{"ratio4",  required_argument, NULL, 'r'},
	{"ratio6",  required_argument, NULL, 'R'},
	{"srclen",  required_argument, NULL, 'l'},
	{"dstlen",  required_argument, NULL, 'L'},
	{"hgi4offset",required_argument, NULL, 'o'},
	{"ps4_network", required_argument, NULL, 'n'},
	{"partialstate4", no_argument, NULL, 'z'},
	{"partialstate6",no_argument, NULL, 'Z'},
	{"icmp_ttl_prefix", no_argument, NULL, 't'},
	{"multiplex6", no_argument, NULL, 'M'},
	{"multiplex4", no_argument, NULL, 'm'},
	{"hgi4",       no_argument, NULL, 'H'},
	{"stateful6",  no_argument, NULL, 's'},
	{"help",       no_argument, NULL, 'h'},
	{"mssclamping",no_argument, NULL, 'c'},
	{"pingbeacon4", no_argument, NULL, 'b'},
	{"pingbeacon6", no_argument, NULL, 'B'},
	{NULL, no_argument, NULL, 0}
};

static int operation = 0; /* 0 for mapping_entry4, 1 for mapping_entry6*/
static bool multi6 = false; /*multiplex 6*/
static bool multi4 = false; /*multiplex 4*/
static bool icmp_ttl_trans = false;
static bool hgi4 = false;
static bool stateful6 = false; /*server 4 stateful*/
static bool mssclamping = false; /*mss clamping*/
static bool pingbeacon4 = false;
static bool pingbeacon6 = false;
static bool ps4 = false;
static bool ps6 = false;
/*mapping_entry4*/
struct in_addr index46;
struct in_addr icmp_ttl_prefix;
struct in_addr ps4prefix;
struct in6_addr srcp46;
struct in6_addr dstp46;
__u8 srcl = 40;
__u8 dstl = 40;
__u16 ratio4 = 1;
__u16 ratio6 = 1;
__u16 hgi4_offset = 0;

/*mapping_entry6*/
struct in6_addr index64;
__u8 mode64 = 0;

void
usage(int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `ivimap --help' for more information.\n");
  else{
	  printf("\
Usage:  ivimap -4 [mapping4_options] IPv4_ADDR\n\
	(used for insert a 4-to-6 mapping)\n\
	ivimap -6 [mapping6_options] IPv6_ADDR\n\
	(user for insert a 6-to-4 mapping)\n\
\n\
4-to-6 mapping4_options:\n\
	-p --srcprefix SRCPREFIX\n\
		specify the source prefix used in the 4-to-6 translation\n\
	-P --dstprefix DSTPREFIX\n\
		specify the destination prefix used in the 4-to-6 translation\n\
\n\
	-l --srclen [SRC PREFIX LENGTH]\n\
		specify the source prefix length used in 4-to-6 translation\n\
	-L --dstlen [DST PREFIX LENGTH]\n\
		specify the destination prefix length used to 4-to-6 translation\n\
	-c --mssclamping\n\
		specify that the 4-to-6 mapping uses tcp mss clamping. \n\
		i.e. reduce 20 in TCP syn message\n\
    V4-side Multiplexing:\n\
	-m --multiplex4\n\
		specify that the IPv4-side network is multiplexed\n\
	-r --ratio4 RATIO\n\
		specify that multiplex ratio of IPv4-side.\n\
		always with -m (--multiplex4), the default is 1\n\
	-b --pingbeacon4\n\
		enable ping beacon on V4 side\n\
	HGI4 mode:\n\
		-H --hgi4\n\
			specify that the IPv4-side multiplexing is used with port shifting\n\
			always with -m (--multiplex4)\n\
		-o --hgi4offset\n\
			specify the offset of hgi4 multiplexing. the default is 0\n\
			always with -m, -H\n\
	Partial State:\n\
		-Z --partialstate6\n\
			ipv6 side of ivi use parital state\n\
		-z --partialstate4\n\
			ipv4 side of ivi use partial state\n\
		-n --ps4_network prefix\n\
			specify the network prefix of hgi connected network, implies -z\n\
    V6-side Multiplexing:\n\
	-M --multiplex6\n\
		specify that the IPv6-side network is multiplexed\n\
		this should not be used with -s (--stateful6)\n\
	-R --ratio6 RATIO\n\
		specify that multiplex ratio of IPv6-side.\n\
		always with -M (--multiplex6), the default is 1\n\
	-s --stateful6\n\
		specify that the IPv6-side NAT is stateful\n\
		this should not be used with -M\n\
	-B --pingbeacon6\n\
		enable ping beacon on V6 side\n\
6-to-4 mapping options:\n\
	-l --srclen [SRC PREFIX LENGTH]\n\
		specify the source prefix length used in 6-to-4 translation\n\
	-L --dstlen [DST PREFIX LENGTH]\n\
		specify the destination prefix length used to 6-to-4 translation\n\
	-t --icmp_ttl_prefix\n\
		translate the source address in icmp-time-exceed packet\n\
    V4-side Multiplexing:\n\
    	-H --hgi4\n\
		specify that the IPv4-side multiplexing is used with port shifting\n\
		always with -m (--multiplex4)\n\
	-b --pingbeacon4\n\
		enable ping beacon on V4 side\n\
	-z --partialstate4\n\
		ipv4 side of ivi use partial state\n\
    V6-side Multiplexing:\n\
	-Z --partialstate6\n\
		ipv6 side of ivi use parital state\n\
	-s --stateful6\n\
		specify that the IPv6-side NAT is stateful\n\
		this should not be used with -M\n\
	-B --pingbeacon6\n\
		enable ping beacon on V6 side\n\
\n\
");
 }
  exit(status);
}
void
insert_m4()
{
	struct mapping_entry4 m4;
	struct ifreq req;
	int sock_fd;
	memset(&m4, 0, sizeof(struct mapping_entry4));
	m4.index = index46;
	m4.srcp = srcp46;
	m4.dstp = dstp46;
	m4.srcl = srcl;
	m4.dstl = dstl;
	m4.src_ratio = ratio4;
	m4.dst_ratio = ratio6;
	m4.hgi4_offset = hgi4_offset;
	if (multi6 && stateful6){
		fprintf(stderr, "6HOST 1:N stateless mode and stateful IVI mode could not exist together\n");
		exit( EXIT_FAILURE );
	}
	if (multi4)
	       m4.mode |= IVI_4HOST_MULTIPLEX;
	if (multi6)
		m4.mode |= IVI_6HOST_MULTIPLEX;
	if (hgi4)
		m4.mode |= IVI_HGI4;
	if (stateful6)
		m4.mode |= IVI_6HOST_STATEFUL;
	if (mssclamping)
		m4.mode |= IVI_MSS_CLAMPING;
	if (pingbeacon4)
		m4.mode |= IVI_PING_BEACON4;
	if (pingbeacon6)
		m4.mode |= IVI_PING_BEACON6;
	if (ps4){
		m4.mode |= IVI_PS4;
		m4.hgi_prefix = ps4prefix.s_addr;
	}
	if (ps6)
		m4.mode |= IVI_PS6;
	m4.error = 0;
	memset(&req, 0, sizeof(struct ifreq));
	strcpy(req.ifr_name, "ivi0");
	req.ifr_data = (char *)&m4;
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(sock_fd, IVI_IOC_ADD4, &req) < 0){
		fprintf(stderr, "mapping_entry4 ioctl error!\n");
		exit( EXIT_FAILURE);
	}
}
void
insert_m6()
{
	struct mapping_entry6 m6;
	struct ifreq req;
	int sock_fd;
	memset(&m6, 0, sizeof(struct mapping_entry6));
	m6.index = index64;
	m6.srcl = srcl;
	m6.dstl = dstl;
	m6.mode = 0;
	if (hgi4)
		m6.mode |= IVI_HGI4;
	if (stateful6)
		m6.mode |= IVI_6HOST_STATEFUL;
	if (pingbeacon4)
		m6.mode |= IVI_PING_BEACON4;
	if (pingbeacon6)
		m6.mode |= IVI_PING_BEACON6;
	if (icmp_ttl_trans){
		m6.mode |= IVI_TRACERT_ADDR_TRANS;
		m6.icmp_ttl_prefix = icmp_ttl_prefix.s_addr;
	}
	if (ps4){
		m6.mode |= IVI_PS4;
	}
	if (ps6)
		m6.mode |= IVI_PS6;
	m6.error = 0;
	memset(&req, 0, sizeof(struct ifreq));
	strcpy(req.ifr_name, "ivi0");
	req.ifr_data = (char *)&m6;
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(sock_fd, IVI_IOC_ADD6, &req) < 0){
		fprintf(stderr, "mapping_entry6 ioctl error!\n");
		exit( EXIT_FAILURE);
	}
}
void
ivimap_init()
{
	inet_pton(AF_INET6, "2001:da9:ff00::", &srcp46);
	inet_pton(AF_INET6, "2001:da9:ff00::", &dstp46);
}
int
main(int argc, char **argv)
{
	int optc, tmp;
	ivimap_init();
	optc = getopt_long(argc, argv, "46h", longopts, NULL);
	switch(optc)
	{
		case '4':
			operation = 0;
			break;
		case '6':
			operation = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "You MUST specify -4 or -6 first\n");
			usage(EXIT_FAILURE);
	}
	while ((optc = getopt_long(argc, argv, "n:t:l:L:p:P:r:R:o:mMhHscbBzZ", longopts, NULL)) != -1)
	{
		switch(optc)
		{
			case 'm':
				multi4 = true;
				break;
			case 'M':
				if(stateful6){
					fprintf(stderr, "stateful mode and multiplex 6 should not be used together\n");
					usage(EXIT_FAILURE);
				}
				multi6 = true;
				break;
			case 'r':
				if (operation){
					fprintf(stderr, "-r should not be used with -6\n");
					usage(EXIT_FAILURE);
				}
				ratio4 = atoi(optarg);
				break;
			case 'R':
				if (operation){
					fprintf(stderr, "-R should not be used with -6\n");
					usage(EXIT_FAILURE);
				}
				ratio6 = atoi(optarg);
				break;
			case 'H': /*implies -m*/
				multi4 = true;
				hgi4 = true;
				break;
			case 's':
				if(multi6){
					fprintf(stderr, "stateful mode and multiplex 6 should not be used together\n");
					usage(EXIT_FAILURE);
				}
				stateful6 = true; /*server 4 stateful*/
				break;
			case 'p':
				if(inet_pton(AF_INET6, optarg, &srcp46) < 0){
					fprintf(stderr, "Please check the address format of src prefix\n");
					usage(EXIT_FAILURE);
				}
				break;
			case 'P':
				if(inet_pton(AF_INET6, optarg, &dstp46) < 0){
					fprintf(stderr, "Pless check the address format of dst prefix\n");
					usage(EXIT_FAILURE);
				}
				break;
			case 'z':
				ps4 = true;
				break;
			case 'Z':
				ps6 = true;
				break;
			case 'n':
				ps4 = true;
				if(inet_aton(optarg, &ps4prefix) == 0){
					fprintf(stderr, "Please check the address format of partial state prefix\n");
					usage(EXIT_FAILURE);
				}
				break;
			case 'o':
				tmp = atoi(optarg);
				hgi4_offset = tmp;
				break;
			case 'l':
				srcl = atoi(optarg);
				break;
			case 'L':
				dstl = atoi(optarg);
				break;
			case 'c':
				mssclamping = true;
				break;
			case 'b':
				pingbeacon4 = true;
				break;
			case 'B':
				pingbeacon6 = true;
				break;
			case 't':
				icmp_ttl_trans = true;
				if (inet_aton(optarg, &icmp_ttl_prefix) == 0){
					fprintf(stderr, "Please check the address format of icmp_ttl_prefix\n");
					usage(EXIT_FAILURE);
				}
				break;
			default:
				usage(EXIT_FAILURE);
		}
	}
	if (optind != argc - 1){
		fprintf(stderr, "There should be one and only one argument\n");
		usage(EXIT_FAILURE);
	}
	if (operation == 0) /*add 4*/{
		if (inet_aton(argv[optind], &index46) == 0){
			fprintf(stderr, "Wrong IPv4 address format\n");
			usage(EXIT_FAILURE);
		}
		insert_m4();
	}
	else{ /*add 6*/
		if (inet_pton(AF_INET6, argv[optind], &index64) < 0){
			fprintf(stderr, "Wrong IPv6 address format\n");
			usage(EXIT_FAILURE);
		}
		insert_m6();
	}
	return EXIT_SUCCESS;
}
