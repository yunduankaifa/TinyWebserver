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

#define BUFFER_SIZE  1024
#define MIN_BUFFER_SIZE     256
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

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
int isSpace(char c) {
    if (c=='\t' || c=='\n' || c == ' ') return 1;
    return 0;

}


void unimplemented(int client)
{
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}


void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */
    
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE *resource)
{
    char buf[1024];
    
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void not_found(int client)
{
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void file_server(int sockfd, char filepath[]) {
    FILE *file = NULL;
    
    file = fopen(filepath, "r");
    if(file==NULL) not_found(sockfd);
    else {
        headers(sockfd, filepath);
        cat(sockfd, file);
    }
    fclose(file);
    

}

void connProc(int *sockfd) {
    char buf[BUFFER_SIZE];
    int  recvlen = -1;
    char method[MIN_BUFFER_SIZE ];
    char url[MIN_BUFFER_SIZE];
    char path[MIN_BUFFER_SIZE];
    int  i=0,j=0;
    int  cgi=0;
    
    recvlen = recv(*sockfd, buf, BUFFER_SIZE-1, 0);
    if (recvlen > 0) {
        printf("%s", buf);
    }
    
    while(i<recvlen && isSpace(buf[i])) i++;
    if (i == recvlen)
        unimplemented(*sockfd);
    
    while(i<recvlen && j<MIN_BUFFER_SIZE && !isSpace(buf[i])) {
        method[j++] = buf[i++];
    }
    if (j<MIN_BUFFER_SIZE && i<recvlen) method[j]='\0';
    else
        unimplemented(*sockfd);

    
    if (strcmp(method, "GET") && strcmp(method, "POST"))
        unimplemented(*sockfd);
    
    if (!strcmp(method, "POST")) cgi=1;
    
    while(i<recvlen && isSpace(buf[i])) i++;
    j=0;
    while(i<recvlen && j<MIN_BUFFER_SIZE && !isSpace(buf[i])) {
        url[j++] = buf[i++];
    }
    
    url[j] = '\0';
    
    if (!cgi) {
        file_server(*sockfd, url);
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
