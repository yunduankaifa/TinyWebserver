//
//  main.c
//  TinyWebserve
//
//  Created by cai on 2017/12/20.
//  Copyright © 2017年 cai. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#define BUFFER_SIZE  1024
#define MIN_BUFFER_SIZE     256
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void error_die(const char*);
int build_response(int, int);
int startServer();
int isSpace(char);
void headers(int, const char *);
void cat(int, FILE *);
void file_server(int, const char *);
void cgi_server(int, const char *);
void connProc(void*);

struct httpResPck {
    int  stateNo;
    char *resContent;
};

struct httpResPck resTable[] = {
    {400, "HTTP/1.0 400 Bad Request\r\nServer: tinywebserver/0.1.0\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE>Bad Request</TITLE>\r\n<BODY><P>The request has wrong, please fix it and try again.\r\n</BODY></HTML>\r\n"},
    {404, "HTTP/1.0 404 NOT FOUND\r\nServer: tinywebserver/0.1.0\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE>Not Found</TITLE>\r\n<BODY><P>The server could not fulfill\r\nyour request because the resource specified\r\nis unavailable or nonexistent.\r\n</BODY></HTML>\r\n"},
    {500, "HTTP/1.0 500 Internal Server Error\r\nServer: tinywebserver/0.1.0\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Internal Server Error\r\n</TITLE></HEAD>\r\n<BODY><P>Internal Server Error.\r\n</BODY></HTML>\r\n"},
    {501, "HTTP/1.0 501 Method Not Implemented\r\nServer: tinywebserver/0.1.0\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n"},
};

//根据状态码state_no构建和传送响应包给client。当返回状态是200时，只构建响应包的首行
int build_response(int state_no, int client) {
    int i;
    int resTypeCnt = (sizeof(resTable)/sizeof(struct httpResPck));
    for (i=0; i<resTypeCnt; i++) {
        if (resTable[i].stateNo == state_no) {
            send(client, resTable[i].resContent, strlen(resTable[i].resContent), 0);
            return  0;
        }
    }
    return -1;
    
}

void error_die(const char *msg) {
    perror(msg);
    exit(1);
}


int startServer() {
    struct sockaddr_in serveraddr;
    int serverSock=-1;
    int on=1;
    
    
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0 ) {
        error_die("start server failed!");
        return -1;
    }
    
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    
    serveraddr.sin_family        = AF_INET;
    serveraddr.sin_port          = htons(8801);
 //   serveraddr.sin_addr.s_addr   = inet_addr(INADDR_ANY);
    serveraddr.sin_addr.s_addr   = htonl(INADDR_ANY);
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        error_die("setsock failed");
        return -1;
    }
    
    if (bind(serverSock, (struct sockaddr*)&serveraddr, (socklen_t)sizeof(struct sockaddr)) != 0) {
        error_die("bind failed!");
        return -1;
    }
    
    if (listen(serverSock, 5) != 0) {
        error_die("listen failed!");
        return -1;
    }
    
    return serverSock;
}

int isSpace(char c) {
    if (c=='\t' || c=='\n' || c == ' ') return 1;
    return 0;

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



void file_server(int client, const char *filepath) {
    FILE *file = NULL;
    file = fopen(filepath, "r");
    if(file==NULL) build_response(501, client);
    else {
        headers(client, filepath);
        cat(client, file);
    }
    fclose(file);
}

void cgi_server(int client, const char *filepath) {
    int cgi_pipe[2]={-1,-1};
    int child_pid=-1;
    char buf[MIN_BUFFER_SIZE];
    int status;
    if(access(filepath, 0)) {
        build_response(404, client);
    } else {
        if (pipe(cgi_pipe)) {  //管道创建失败
            build_response(500, client);
        }
        
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
        send(client, buf, strlen(buf), 0);
        if ((child_pid=fork()) < 0) {
            //创建子进程失败
        } else if (child_pid > 0) {
            //父进程
            close(cgi_pipe[1]);
            while(read(cgi_pipe[0], buf, MIN_BUFFER_SIZE)) {
                send(client, buf, sizeof(buf), 0);
            }
            close(cgi_pipe[0]);
            waitpid(child_pid, &status, 0);
  
            printf("%s", "father process ended! \n");
  
            
        } else {
            //子进程
            dup2(cgi_pipe[1],STDOUT_FILENO);
            sprintf(buf, "QUERY_STRING=%s", filepath);
            putenv(buf);
            close(cgi_pipe[0]);
            if (execl(filepath, NULL)<0) {
                build_response(500, client);
            }
            exit(0);
        }
    }
}

void connProc(void *arg) {
    char buf[BUFFER_SIZE];
    ssize_t  recvlen = 0;
    char method[MIN_BUFFER_SIZE ];
    char url[MIN_BUFFER_SIZE];
    int  i=0,j=0;
    int  cgi=0;
    int client  = *((int*)arg);
    
    recvlen = recv(client, buf, BUFFER_SIZE-1, 0);
    if (recvlen < 0) {
        build_response(400, client);
    }
    
    
    while(i<recvlen && isSpace(buf[i])) i++;
    if (i == recvlen)
        build_response(400, client);
    
    while(i<recvlen && j<MIN_BUFFER_SIZE && !isSpace(buf[i])) {
        method[j++] = buf[i++];
    }
    if (j<MIN_BUFFER_SIZE && i<recvlen) method[j]='\0';
    else
       build_response(400, client);

    
    if (strcmp(method, "GET") && strcmp(method, "POST"))
       build_response(400, client);
    
    if (!strcmp(method, "POST")) cgi=1;
    
    while(i<recvlen && isSpace(buf[i])) i++;
    
    j=0;
    while(i<recvlen && j<MIN_BUFFER_SIZE && !isSpace(buf[i])) {
        url[j++] = buf[i++];
    }
    
    url[j] = '\0';
    
    if (!cgi) {
        file_server(client, url);
    } else {
        cgi_server(client, url);
    }
    
    close(client);
  

}


int main(int argc, const char * argv[]) {
    int serverSock=0;
    int acceptSock;
    int cliendaddrLen;
    pthread_t pid;
    struct sockaddr_in clientaddr;
    
    
    if ((serverSock=startServer()) < 0)
        error_die("start server failed!");
    
    
    
    while(1) {
        acceptSock = accept(serverSock, (struct sockaddr*)&clientaddr, (socklen_t*)&cliendaddrLen);
        if (acceptSock < 0) {
            perror("accept client failed\n");
            continue;
        }
        
        if (pthread_create(&pid, NULL, (void *)&connProc, (void *)&acceptSock) != 0 ) {
            perror("client conn failed\n");
            close(acceptSock);
            continue;
        }

    
    }
    
    
    return 0;
}
