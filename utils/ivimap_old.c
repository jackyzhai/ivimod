/* Command Line Interface for IVI
 * 2009-6-29	Hong Zhang
 * Usage: ivimap <next hop index> <src prefix> <prefix length> <dst prefix> <prefix length> <multiplex ratio> <offset> 
 * Netmask 255.255.255.255 will be automatically viewed as host route
 * 	   0.0.0.0 will be automatically viewed as default route
 */

#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <linux/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/route.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <stdio.h>

#include "ivimap.h"

int main(int argc, void *argv[])
{	
	struct in_addr index;
	struct mapping_entry4 map4;
	struct mapping_entry4 * ret4;
	struct mapping_entry6 map6;
	struct mapping_entry6 * ret6;
	struct ifreq req;
	int sock_fd;

	if (argc == 8)
		map4.offset = atoi(argv[7]);
	else if (argc == 7)
		map4.offset = 0xFFFF;
	else if (argc == 4)
		map6.offset = 0;
	else if (argc == 5)
		map6.offset = atoi(argv[4]);
	else
	{
		printf("Invalid argument!\n");
		return 0;
	}

	if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("Socket Error!\n");
		return 0;
	}
	
	if (argc == 4 || argc == 5)
	{
	
	if (inet_pton(AF_INET6, argv[1], &(map6.index)) < 0)
	{
		printf("Wrong IPv6 Gateway!\n");
        	return 0;	
	}

	map6.srcl=atoi(argv[2]);
	map6.dstl=atoi(argv[3]);

	map6.error = 0;

	strncpy(req.ifr_name, "ivi0", sizeof(req.ifr_name)-1);
	req.ifr_data = (char *)&map6;	


	if (ioctl(sock_fd, IVI_IOC_ADD6, &req) < 0)
	{
		printf("ioctl error!\n");
		return 0;
	}
	
	close(sock_fd);

	ret6 = (struct mapping_entry6 *)(req.ifr_data);
	if (ret6->error != 0)
	{
		printf("Error No.%u\n",ret6->error);
		return 0;
	}
	return 1;
	
	}

	else
	{

	inet_aton(argv[1], &(map4.index));

	if (inet_pton(AF_INET6, argv[2], &(map4.srcp)) < 0)
	{
        printf("Wrong source prefix!\n");
        return 0;
    	}
	map4.srcl=atoi(argv[3]);

	if (inet_pton(AF_INET6, argv[4], &(map4.dstp)) < 0)
	{
        printf("Wrong destination prefix!\n");
        return 0;
    	}
	map4.dstl = atoi(argv[5]);
	map4.ratio = atoi(argv[6]);
	map4.error = 0;

	strncpy(req.ifr_name, "ivi0", sizeof(req.ifr_name)-1);
	req.ifr_data = (char *)&map4;	


	if (ioctl(sock_fd, IVI_IOC_ADD4, &req) < 0)
	{
		printf("ioctl error!\n");
		return 0;
	}
	
	close(sock_fd);

	ret4 = (struct mapping_entry4 *)(req.ifr_data);
	if (ret4->error != 0)
	{
		printf("Error No.%u\n",ret4->error);
		return 0;
	}
	return 1;
	
	}
}
