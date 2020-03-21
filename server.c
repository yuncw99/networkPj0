#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"

#define BUF_LEN 10*1024*1024
#define ASCII_A 97

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
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char *buf = malloc(BUF_LEN);
    int res_len, cli_addr_len;
    int n = 2;
    unsigned short tmp_checksum = 0;

    struct Protocol protocol;

    if((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("can't create socket\n");
        exit(0);
    }

    memset((char *)&server_addr, 0x00, sizeof(server_addr));

    for(int i=1; i<argc; i+=2) {
        if(!strcmp(argv[i], "-p")) {
            server_addr.sin_port = htons(atoi(argv[i+1]));
        } else {
            printf("input error\n");
            exit(0);
        }
    }

    server_addr.sin_family = AF_INET;
    //server_addr.sin_port = htons(12000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Can't bind\n");
        exit(0);
    }

    if(listen(server_fd, 5) < 0) {
        printf("Can't listen\n");
        exit(0);
    }

    cli_addr_len = sizeof(client_addr);

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &cli_addr_len);
    if(client_fd < 0){
        printf("Can't access to client\n");
        exit(0);
    }

    printf("Client %d connected\n", client_fd);

    while(1) {
        res_len = read(client_fd, buf, BUF_LEN);

        memcpy((char *)&protocol, buf, sizeof(protocol));

        printf("op : %d, shift : %d, checksum : %x\n", protocol.op, protocol.shift, protocol.checksum);
        tmp_checksum = protocol.checksum;
        protocol.checksum = 0;
        memcpy(buf, (char *)&protocol, sizeof(protocol));

        if(checksum2(buf, ntohl(protocol.length)) != tmp_checksum) {
            printf("different checksum : %x, %x\n", checksum2(buf, ntohl(protocol.length)), tmp_checksum);
            exit(0);
        }

        for(int i=8; i<res_len; i++) {
            if(isupper(buf[i]))
                buf[i] = tolower(buf[i]);

            if(isalpha(buf[i])) {
                if(protocol.op == 0) {
                    buf[i] = (char)((((int)buf[i] + protocol.shift - ASCII_A + 26) % 26) + ASCII_A);
                } else if(protocol.op == 1) {
                    buf[i] = (char)((((int)buf[i] - protocol.shift - ASCII_A + 26) % 26) + ASCII_A);
                }
            }
        }

        protocol.checksum = checksum2(buf, ntohl(protocol.length));
        memcpy(buf, (char *)&protocol, sizeof(protocol));

        write(client_fd, buf, res_len);
    }

    close(client_fd);
    close(server_fd);
    free(buf);

    return 0;
}