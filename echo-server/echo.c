/*
 * echo - read and echo text lines until client closes connection
          클라이언트가 연결을 닫을 때까지 텍스트 줄을 읽고 에코한다.
 */
/* $begin echo */
#include "csapp.h"

void echo(int connfd) 
{
    size_t n;       //현재 수신된 데이터의 크기 저장
    char buf[MAXLINE];  //수신된 데이터를 저장하는 버퍼
    rio_t rio;      //타입의 구조체로, 안전한 방식으로 데이터를 읽고 쓰는데 사용되는 리오 버퍼

    Rio_readinitb(&rio, connfd);    //리오 구조체 초기화
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { //line:netp:echo:eof
	printf("server received %d bytes\n", (int)n);
	Rio_writen(connfd, buf, n);     //클라이언트에게 데이터 다시 전송. buf에 있는 데이터를 소켓으로 전송
    }
}
/* $end echo */
