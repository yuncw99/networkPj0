#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"

#define BUF_LEN 10000000

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
    int sent_len = 0, res_len = 0, temp_len = 0;
    struct Protocol protocol;
    char *buf = malloc(BUF_LEN);
    char *packet = malloc(BUF_LEN);
    unsigned short tmp_checksum = 0;
    char tmp_char;
    int i;

    int message_finish = 0;

    if((server = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("can't create socket\n");
        exit(0);
    }

    memset((char *)&server_addr, 0x00, sizeof(server_addr));

    for(i=1; i<argc; i+=2) {
        if(!strcmp(argv[i], "-h")) {
            server_addr.sin_addr.s_addr = inet_addr(argv[i+1]);
        } else if(!strcmp(argv[i], "-p")) {
            server_addr.sin_port = htons(atoi(argv[i+1]));
        } else if(!strcmp(argv[i], "-o")) {
            protocol.op = atoi(argv[i+1]);
        } else if(!strcmp(argv[i], "-s")) {
            if(atoi(argv[i+1]) >= 0) {
                protocol.shift = atoi(argv[i+1]) % 26;
            } else {
                protocol.shift = 26 - ((0 - atoi(argv[i+1])) % 26);
            }
        } else {
            printf("input error\n");
            exit(0);
        }
    }

    server_addr.sin_family = AF_INET;
    //printf("op : %d, shift : %d\n", protocol.op, protocol.shift);

    if(connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("can't connect\n");
        exit(0);
    }
    printf("connected. %d\n", server);

    // input
    while(!message_finish) {
        for(i=0; i<BUF_LEN-10; i++) {
            tmp_char = fgetc(stdin);
            buf[i] = tmp_char;

            if(tmp_char == EOF) {
                message_finish = 1;
                break;
            }
        }
        buf[i+1] = 0;
        //message_finish = 1;

        // write packet
        protocol.checksum = 0;
        printf("write. strlen : %d\n", strlen(buf));
        protocol.length = htonl(strlen(buf) + 8);

        memcpy(packet, (char*)&protocol, sizeof(protocol));
        strcpy(packet+8, buf);
        packet[ntohl(protocol.length)] = 0;

        protocol.checksum = checksum2(packet, ntohl(protocol.length));
        printf("length : %d, checksum : %x\n", ntohl(protocol.length), protocol.checksum);

        memcpy(packet, (char*)&protocol, sizeof(protocol));

        write(server, packet, ntohl(protocol.length));

        //read packet
        while(res_len < 8) {
            temp_len = read(server, packet + res_len, BUF_LEN);
            res_len += temp_len;
            //printf("read. res_len : %d, temp_len : %d\n", res_len, temp_len);

            if(temp_len == 0 || temp_len == -1) {
                printf("disconnected. \n");
                exit(0);
            }
        }

        memcpy((char *)&protocol, packet, sizeof(protocol));

        while(res_len < ntohl(protocol.length)) {
            temp_len = read(server, packet + res_len, ntohl(protocol.length) - res_len);
            res_len += temp_len;
            //printf("read. res_len : %d, temp_len : %d\n", res_len, temp_len);

            if(temp_len == 0 || temp_len == -1) {
                printf("disconnected. \n");
                exit(0);
            }
        }
        packet[res_len] = 0;

        tmp_checksum = protocol.checksum;
        protocol.checksum = 0;
        memcpy(packet, (char *)&protocol, sizeof(protocol));

        if(checksum2(packet, ntohl(protocol.length)) != tmp_checksum) {
            printf("different checksum : %x, %x\n", checksum2(packet, ntohl(protocol.length)), tmp_checksum);
        } else {
            printf("read. res_len : %d\n", res_len);
            printf("%s", packet+8);
        }
        res_len = 0;
    }

    // terminate
    close(server);
    free(buf);
    free(packet);

    return 0;
}