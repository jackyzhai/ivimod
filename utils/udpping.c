#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
int main()
{
	int sendsd;
	struct sockaddr_in6 srcaddr;
	struct sockaddr_in6 dstaddr;
	char *s = "haha, just for test";
	if ((sendsd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0){
		printf("err ");
		exit(1);
	}
	memset(&srcaddr, 0, sizeof(srcaddr));
	srcaddr.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "2001:da9:ff01:0102:0100::", &srcaddr.sin6_addr);
	srcaddr.sin6_port = htons(5235);
	memset(&dstaddr, 0, sizeof(dstaddr));
	dstaddr.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "2001:da9:ff01:0101:0200::", &dstaddr.sin6_addr);
	dstaddr.sin6_port = htons(5235);
	if (bind(sendsd, (struct sockaddr *)(&srcaddr), sizeof(srcaddr)) < 0)
	{
		printf(" recv sock bind err");
		exit(1);
	}
	sendto(sendsd, s, 10, 0, &dstaddr, sizeof(dstaddr)); 
}
