/*
 * adder.c - 두 숫자를 더하는 최소한의 CGI 프로그램
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) //환경 변수에서 쿼리 문자열을 가져온다.
  {
    p = strchr(buf, '&'); //쿼리 문자열에서 첫 번째 &문자를 찾아 그 위치를 반환한다.
    *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p + 1);
    // n1 = atoi(arg1);  //문자열을 정수로 변환하는 함수
    // n2 = atoi(arg2);  
    
    /* adder 함수 */
    sscanf(buf, "n1=%d", &n1);
    sscanf(p+1, "n2=%d", &n2);
  }

  /* Make the response body */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout); //출력 버퍼 비워서 모든 내용 즉시 클라이언트로 보냄

  exit(0);
}
/* $end adder */