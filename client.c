#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"

#define BUF_LEN 10*1024*1024

struct Protocol {
    char op;
    char shift;
    unsigned short checksum;
    int length;
};

unsigned short checksum2(const char *buf, unsigned size)
{
	unsigned long long sum = 0;
	const unsigned long long *b = (unsigned long long *) buf;

	unsigned t1, t2;
	unsigned short t3, t4;

	/* Main loop - 8 bytes at a time */
	while (size >= sizeof(unsigned long long))
	{
		unsigned long long s = *b++;
		sum += s;
		if (sum < s) sum++;
		size -= 8;
	}

	/* Handle tail less than 8-bytes long */
	buf = (const char *) b;
	if (size & 4)
	{
		unsigned s = *(unsigned *)buf;
		sum += s;
		if (sum < s) sum++;
		buf += 4;
	}

	if (size & 2)
	{
		unsigned short s = *(unsigned short *) buf;
		sum += s;
		if (sum < s) sum++;
		buf += 2;
	}

	if (size)
	{
		unsigned char s = *(unsigned char *) buf;
		sum += s;
		if (sum < s) sum++;
	}

	/* Fold down to 16 bits */
	t1 = sum;
	t2 = sum >> 32;
	t1 += t2;
	if (t1 < t2) t1++;
	t3 = t1;
	t4 = t1 >> 16;
	t3 += t4;
	if (t3 < t4) t3++;

	return ~t3;
}

int main(int argc, char *argv[]) {
    int server;
    struct sockaddr_in server_addr;
    int res_len;
    struct Protocol protocol;
    char *buf = malloc(BUF_LEN-8);
    char *packet = malloc(BUF_LEN);

    if((server = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("can't create socket\n");
        exit(0);
    }

    memset((char *)&server_addr, 0x00, sizeof(server_addr));

    for(int i=1; i<argc; i+=2) {
        if(!strcmp(argv[i], "-h")) {
            server_addr.sin_addr.s_addr = inet_addr(argv[i+1]);
        } else if(!strcmp(argv[i], "-p")) {
            server_addr.sin_port = htons(atoi(argv[i+1]));
        } else if(!strcmp(argv[i], "-o")) {
            protocol.op = atoi(argv[i+1]);
        } else if(!strcmp(argv[i], "-s")) {
            protocol.shift = atoi(argv[i+1]);
        } else {
            printf("input error\n");
            exit(0);
        }
    }

    server_addr.sin_family = AF_INET;
    //server_addr.sin_port = htons(12000);
    //server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //printf("op : %d, shift : %d\n", protocol.op, protocol.shift);

    if(connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("can't connect\n");
        exit(0);
    }

    printf("connected. %d\n", server);

    while(1) {
        gets(buf);
        protocol.checksum = 0;
        protocol.length = htonl(strlen(buf) + 8);

        memcpy(packet, (char*)&protocol, sizeof(protocol));
        strcpy(packet+8, buf);

        protocol.checksum = checksum2(packet, ntohl(protocol.length));
        printf("checksum : %x\n", protocol.checksum);

        memcpy(packet, (char*)&protocol, sizeof(protocol));

        write(server, packet, ntohl(protocol.length));

        res_len = read(server, buf, ntohl(protocol.length));
        printf("res_len : %d\n", res_len);
        buf[res_len] = 0;

        printf("%s\n", buf+8);
    }

    close(server);
    free(buf);
    free(packet);

    return 0;
}