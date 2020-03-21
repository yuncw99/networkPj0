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
    short checksum;
    int length;
};

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

    printf("op : %d, shift : %d\n", protocol.op, protocol.shift);

    if(connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("can't connect\n");
        exit(0);
    }

    printf("connected. %d\n", server);

    while(1) {
        //scanf("%s", buf);
        gets(buf);

        protocol.length = strlen(buf) + 8;

        memcpy(packet, (char*)&protocol, sizeof(protocol));
        strcpy(packet+8, buf);

        //write(server, buf, strlen(buf));
        write(server, packet, protocol.length);

        res_len = read(server, buf, BUF_LEN);
        printf("res_len : %d\n", res_len);
        buf[res_len] = 0;
        //printf("%s\n", buf);
        printf("%s\n", buf+8);
    }

    close(server);
    free(buf);
    free(packet);

    return 0;
}