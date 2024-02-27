/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n",argv[0]);
        exit(0);
    }
    host = argv[1]; //호스트 
    port = argv[2]; //포트 번호

    clientfd = Open_clientfd(host, port);   //클라이언트 소켓을 열어서 서버와 연결
    Rio_readinitb(&rio, clientfd);  //클라이언트 소켓을 통해 데이터를 읽을 리오 버퍼 초기화

    while (Fgets(buf, MAXLINE, stdin) != NULL) {   //사용자로부터 표준입력을 통해 한 줄씩 입력 받는다.
        Rio_writen(clientfd, buf, strlen(buf)); //클라이언트 소켓을 통해 버퍼에 있는 데이터를 서버로 보낸다.
        Rio_readlineb(&rio, buf, MAXLINE);  //클라이언트 소켓을 통해 서버에서의 응답을 읽어들인다.
        Fputs(buf, stdout); //읽어들인 응답을 출력
    }
    Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */