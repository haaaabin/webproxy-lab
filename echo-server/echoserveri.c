/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv) 
{
    int listenfd, connfd; 
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  //sockaddr_storage 선언 이유 - 모든 형태의 소켓 주소를 저장하기에 충분히 크며, 
    char client_hostname[MAXLINE], client_port[MAXLINE];  

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);  //주어진 포트에서 들어오는 연결 요쳥을 받을 준비가 된 듣기 식별자 오픈
    while (1) { 
        clientlen = sizeof(struct sockaddr_storage);    //클라이언트 주소 구조체의 크기 설정
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   //클라이언트의 연결을 수락하고 클라이언트 소켓 디스크립터 반환
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);   //클라이언트의 주소를 호스트 이름과 포트번호로 변환
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);
    }
    exit(0);
}
/* $end echoserverimain */