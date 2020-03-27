#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include <unistd.h>

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

int accept_repeat(int fd, struct sockaddr *addr, socklen_t *addr_len) {
    int client_fd = 0;

    client_fd = accept(fd, addr, addr_len);
    if(client_fd < 0){
        //printf("Can't access to client\n");
        exit(0);
    } else {
        //printf("Client %d connected\n", client_fd);
    }

    return client_fd;
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char *buf = malloc(BUF_LEN);
    int res_len = 0, cli_addr_len, temp_len;
    int n = 2;
    unsigned short tmp_checksum = 0;
    int message_finish = 0;
    int pid;

    struct Protocol protocol;

    if((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        //printf("can't create socket\n");
        exit(0);
    }

    memset((char *)&server_addr, 0x00, sizeof(server_addr));

    for(int i=1; i<argc; i+=2) {
        if(!strcmp(argv[i], "-p")) {
            server_addr.sin_port = htons(atoi(argv[i+1]));
        } else {
            //printf("input error\n");
            exit(0);
        }
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        //printf("Can't bind\n");
        exit(0);
    }

    if(listen(server_fd, 5) < 0) {
        //printf("Can't listen\n");
        exit(0);
    }

    cli_addr_len = sizeof(client_addr);

    while(1) {
        client_fd = accept_repeat(server_fd, (struct sockaddr *)&client_addr, &cli_addr_len);

        pid = fork();

        if(pid > 0) {
            close(client_fd);
            continue;
        }

        //printf("child process - pid : %d\n", pid);
        while(!message_finish) {
            res_len = 0;

            while(res_len < 8) {
                temp_len = read(client_fd, buf + res_len, BUF_LEN);
                res_len += temp_len;
                //printf("res_len : %d, temp_len : %d\n", res_len, temp_len);

                if(temp_len == 0 || temp_len == -1) {
                    //printf("disconnect : %d\n", client_fd);
                    close(client_fd);
                    client_fd = 0;

                    break;
                }
            }
            if(client_fd == 0)
                break;

            memcpy((char *)&protocol, buf, sizeof(protocol));

            while(res_len < ntohl(protocol.length)) {
                temp_len = read(client_fd, buf + res_len, ntohl(protocol.length) - res_len);
                res_len += temp_len;
                //printf("res_len : %d, temp_len : %d\n", res_len, temp_len);

                if(temp_len == 0 || temp_len == -1) {
                    //printf("disconnect : %d\n", client_fd);
                    close(client_fd);
                    client_fd = 0;

                    break;
                }
            }
            if(client_fd == 0)
                break;

            buf[res_len] = 0;
            if(buf[res_len-1] == EOF) {
                //printf("last message!\n");
                message_finish = 1;
            }

            // check whether valid checksum
            memcpy((char *)&protocol, buf, sizeof(protocol));

            //printf("length : %d, checksum : %x\n", ntohl(protocol.length), protocol.checksum);
            tmp_checksum = protocol.checksum;
            protocol.checksum = 0;
            memcpy(buf, (char *)&protocol, sizeof(protocol));

            if(!(protocol.op == 0 || protocol.op == 1) || checksum2(buf, ntohl(protocol.length)) != tmp_checksum) {
                //printf("different checksum : %x, %x\n", checksum2(buf, ntohl(protocol.length)), tmp_checksum);

                message_finish = 1;
                close(client_fd);
                client_fd = 0;
            } else {
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
                //printf("write completed. %d\n", client_fd);
            }
        }

        message_finish = 0;
        free(buf);
        //printf("child process exit.\n", pid);

        if(client_fd != 0) {
            close(client_fd);
            close(server_fd);
            exit(0);
        } else {
            close(server_fd);
            exit(-1);
        }
    }

    close(client_fd);
    close(server_fd);
    free(buf);

    return 0;
}