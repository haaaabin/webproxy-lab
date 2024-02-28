#include <stdio.h>
#include "csapp.h"

void *thread(void *vargp);
void doit(int clientfd);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

static const int is_local_test = 0; // 테스트 환경에 따른 도메인&포트 지정을 위한 상수 (0 할당 시 도메인&포트가 고정되어 외부에서 접속 가능)
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, *clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;  //새로 생성된 스레드의 id 저장 포인터

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 전달받은 포트 번호를 사용해 수신 소켓 생성
  while (1)
  {
    clientlen = sizeof(clientaddr); 
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 요청 수신
    
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    
    //concurrent requests proxy
    //각 클라이언트 연결에 대해 새로운 스레드를 생성하여, 여러 클라이언트 요청을 동시에 처리할 수 있다.
    Pthread_create(&tid, NULL, thread, clientfd);
    
    // doit(clientfd);
    // Close(clientfd);
  }
  return 0;
}

//새로 생성된 스레드에서 실행되는 함수. 클라이언트의 요청 처리 수행
//vargp는 Pthread_create 함수 호출 시 thread 함수에 전달된 인자, 클라이언트와의 연결을 나타내는 파일 디스크립터
void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);   // int포인터로 변환한 후 , 그 값을 clientfd로 복사
  Pthread_detach(pthread_self());   //새로 생성된 스레드를 분리(detach). 분리된 스레드는 자동으로 정리
  Free(vargp);
  doit(clientfd); 
  Close(clientfd);
  return NULL;
}

void doit(int clientfd)
{
  int serverfd, content_length; //서버 fd, HTTP응답의 내용 길이
  char request_buf[MAXLINE], response_buf[MAXLINE]; //HTTP 요청과 응답을 저장하는 버퍼
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];  //메소드, uri, 파일 경로, 호스트 이름, 포트 번호
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE];  //응답 버퍼를 가리키는 버퍼, 요청된 파일 이름, CGI스크립트에 전달될 인자
  rio_t request_rio, response_rio;  //요청과 응답을 위한 rio 구조체

  /*1) Request Line 읽기 [Client -> Proxy] */
  Rio_readinitb(&request_rio, clientfd);    // clientfd를 이용해 리오 버퍼 초기화
  Rio_readlineb(&request_rio, request_buf, MAXLINE);  //request_rio를 통해 요청 라인을 읽어 request_buf에 저장
  printf("Request headers:\n %s\n", request_buf);

  // 요청 라인 parsing을 통해 `method, uri, hostname, port, path` 찾기
  sscanf(request_buf, "%s %s", method, uri);  //request_buf에서 HTTP 메소드와 uri를 파싱해 method와 uri에 저장
  parse_uri(uri, hostname, port, path); //uri 파싱해 호스트 이름, 포트 경로를 찾아 hostname, port,path에 저장

  // Server에 전송하기 위해 요청 라인의 형식 변경: `method uri version` -> `method path HTTP/1.0`
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

  // 지원하지 않는 method인 경우 예외 처리
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* 2) Request Line 전송 [Proxy -> Server] */
  // Server 소켓 생성
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("127.0.0.1", port);  //만약 로컬에서 테스트 중이라면 주어진 호스트 이름과 포트로 연결, 그렇지 않다면 로컬호스트에 연결 생성
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", " Failed to establish connection with the end server");
    return;
  }

  Rio_writen(serverfd, request_buf, strlen(request_buf)); //서버에 요청 라인 전송

  /* Request Header 읽기 & 전송 [Client -> Proxy ->  Server] */
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);  //클라이언트의 요청 헤더를 읽어서 이를 서버에 전송

  /* Response Header 읽기 & 전송 [Server -> Proxy -> Client] */
  Rio_readinitb(&response_rio, serverfd); //serverfd를 이용해 리오 버퍼 초기화
  while (strcmp(response_buf, "\r\n"))
  {
    Rio_readlineb(&response_rio, response_buf, MAXLINE);        // response_rio를 통해 요청 라인을 읽어 response_buf에 저장
    if (strstr(response_buf, "Content-length"))                 // Response Body 수신에 사용하기 위해 Content-length 저장
      content_length = atoi(strchr(response_buf, ':') + 1);     // 이 헤더가 있다면, atoi함수와 strchr 함수를 사용하여 헤더의 값에서 콘텐츠의 길이 추출
    Rio_writen(clientfd, response_buf, strlen(response_buf));   // clientfd로 전송
  }

  /* Response Body 읽기 & 전송 [Server -> Proxy -> Client] */
  response_ptr = malloc(content_length);
  Rio_readnb(&response_rio, response_ptr, content_length);
  Rio_writen(clientfd, response_ptr, content_length); // Client에 Response Body 전송
  
  free(response_ptr);
  Close(serverfd);
}

// 클라이언트에 에러를 전송하는 함수
// cause: 오류 원인, errnum: 오류 번호, shortmsg: 짧은 오류 메시지, longmsg: 긴 오류 메시지
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // 에러 Bdoy 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // 에러 Header 생성 & 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 에러 Body 전송
  Rio_writen(fd, body, strlen(body));
}

// uri를 `hostname`, `port`, `path`로 파싱하는 함수
// uri 형태: `http://hostname:port/path` 혹은 `http://hostname/path` (port는 optional)
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // host_name의 시작 위치 포인터: '//'가 있으면 //뒤(ptr+2)부터, 없으면 uri 처음부터
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  char *port_ptr = strchr(hostname_ptr, ':'); // port 시작 위치 (없으면 NULL)
  char *path_ptr = strchr(hostname_ptr, '/'); // path 시작 위치 (없으면 NULL)
  strcpy(path, path_ptr);

  if (port_ptr) // port 있는 경우
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1); 
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }
  else // port 없는 경우
  {
    if (is_local_test)
      strcpy(port, "80"); // port의 기본 값인 80으로 설정
    else
      strcpy(port, "8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
  printf("# Parsed -> hostname: %s, port: %s, path: %s\n", hostname, port, path);
}

// Request Header를 읽고 Server에 전송하는 함수
// 필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
  int is_host_exist;
  int is_connection_exist;
  int is_proxy_connection_exist;
  int is_user_agent_exist;

  Rio_readlineb(request_rio, request_buf, MAXLINE); // 첫 번째 줄 읽기(HTTP 요청의 첫 번째 헤더)
  while (strcmp(request_buf, "\r\n"))
  {
    if (strstr(request_buf, "Proxy-Connection") != NULL)
    {
      sprintf(request_buf, "Proxy-Connection: close\r\n");
      is_proxy_connection_exist = 1;  //헤더의 존재 표시
    }
    else if (strstr(request_buf, "Connection") != NULL)
    {
      sprintf(request_buf, "Connection: close\r\n");
      is_connection_exist = 1;
    }
    else if (strstr(request_buf, "User-Agent") != NULL)
    {
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }
    else if (strstr(request_buf, "Host") != NULL)
    {
      is_host_exist = 1;
    }

    Rio_writen(serverfd, request_buf, strlen(request_buf)); // Server에 전송
    Rio_readlineb(request_rio, request_buf, MAXLINE);       // 다음 줄 읽기
  }

  // 필수 헤더 미포함 시 추가로 전송
  if (!is_proxy_connection_exist)
  {
    sprintf(request_buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_connection_exist)
  {
    sprintf(request_buf, "Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_host_exist)
  {
    if (!is_local_test) //테스트 중이 아닐 때
      hostname = "127.0.0.1";
    sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_user_agent_exist)
  {
    sprintf(request_buf, user_agent_hdr);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }

  sprintf(request_buf, "\r\n"); // 종료문
  Rio_writen(serverfd, request_buf, strlen(request_buf));
  return;
}