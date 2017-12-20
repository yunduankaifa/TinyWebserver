//
//  main.c
//  TinyWebserve
//
//  Created by cai on 2017/12/20.
//  Copyright © 2017年 cai. All rights reserved.
//

#include <stdio.h>
#include <sys/socket.h>
#include <sys/netport.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>


void error_die(const char *msg) {
    perror(msg);
    exit(1);
}


int startServer(int sockfd) {
    struct sockaddr_in serveraddr, clientaddr;
    int on=1;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    
    serveraddr.sin_family        = AF_INET;
    serveraddr.sin_port          = htons(8801);
    serveraddr.sin_addr.s_addr   = inet_addr("127.0.0.1");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        error_die("setsock failed");
    
    if (bind(sockfd, (struct sockaddr*)&serveraddr, (socklen_t)sizeof(struct sockaddr)) != 0)
        error_die("bind failed!");
    
    if (listen(sockfd, 5) != 0)
        error_die("listen failed!");
    
    return 0;


}

void connProc(int *sockfd) {
    char buf[256];
    int recvlen = -1;
    
    recvlen = recv(*sockfd, buf, 255, 0);
    if (recvlen > 0) {
        printf("%s", buf);
    }
    
    
    printf("%s", "client proc!");

}


int main(int argc, const char * argv[]) {
    int listensock=0;
    int acceptsock;
    int cliendaddr_len;
    int pid;
    struct sockaddr_in clientaddr;
 
    printf("run success\n");
    
    
    listensock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (listensock < 0 )
        error_die("start server failed!");

    if (startServer(listensock) != 0)
        error_die("start server failed!");
    
    
    
    while(1) {
        acceptsock = accept(listensock, (struct sockaddr*)&clientaddr, (socklen_t*)&cliendaddr_len);
        if (acceptsock < 0) {
            perror("accept client failed\n");
            continue;
        }
        

        
        if (pthread_create(&pid, NULL, &connProc, (void *)&acceptsock) != 0 ) {
            perror("client conn failed\n");
            close(acceptsock);
            continue;
        }

    
    }
    
    
    return 0;
}
