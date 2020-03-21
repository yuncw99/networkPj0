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
    short checksum;
    int length;
};

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char *buf = malloc(BUF_LEN);
    int res_len, cli_addr_len;
    int n = 2;

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

    while(1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &cli_addr_len);
        if(client_fd < 0){
            printf("Can't access to client\n");
            exit(0);
        }

        printf("Client %d connected\n", client_fd);

        res_len = read(client_fd, buf, BUF_LEN);

        protocol.op = buf[0];
        protocol.shift = buf[1];

        printf("op : %d, shift : %d\n", protocol.op, protocol.shift);

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

        write(client_fd, buf, res_len);
    }

    close(client_fd);
    close(server_fd);
    free(buf);

    return 0;
}