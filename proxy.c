#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

//테스트 환경에 따른 도메인&포트 지정을 위한 상수(0 할당 시 도메인&포트가 고정되어 외부에서 접속 가능)
static const int is_local_test = 1;
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
  return 0;
}

void doit(int fd)
{
  int serverfd, content_length;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE];
  rio_t request_rio, reponse_rio;

  /* 1) Request Line 읽기 (Client -> Proxy) */
  Rio_readinitb(&request_rio, fd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", request_buf);
  sscanf(request_buf, "%s %s ", method, uri);
  
  // 요청 라인 parsing을 통해 'method, uri, hostname, port, path' 찾기
  parse_uri(uri, hostname, port, path);

  //Server에 전송하기 위해 요청 라인의 형식 변경
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

  //지원하지 않는 method인 경우 예외 처리
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* 2 ) Request Line 전송 (Proxy -> Server)*/
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_cilentfd("127.0.0.1", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", " Failed to establish connection with the end server");
    return;
  }

  Rio_writen(serverfd, request_buf , strlen(request_buf));

  /* Request header 읽기 & 전송 (Client -> Proxy -> Server)*/
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);

 
}



/* 클라이언트에게 HTTP 오류 메시지를 보내는 함수*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /*Build the HTTP response body*/
  // sprintf(문자열을 저장할 버퍼, 출력할 값)
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor = "
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>/r/n", body);

  /*Print the HTTP response*/
  sprintf(buf, "%s %s %s\r\n", http_version, errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html/\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// Request Header를 읽고 server에 전송하는 함수
// 필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송
void read_requesthdrs(rio_t *rp, void *request_buf, int serverfd, char *hostname, char *port)
{
  int is_host_exist;   
  int is_connection_exist; 
  int is_proxy_connection_exist;
  int is_user_agent_exist;

  Rio_readlineb(rp, request_buf, MAXLINE);
  while (strcmp(request_buf, "\r\n"))
  {
    if(strcmp(request_buf, "Proxy-Connection")!= NULL)
    {
      sprintf(request_buf, "Proxy-Connection: close\r\n");
      is_proxy_connection_exist = 1;
    }
    else if(strstr(request_buf, "Connection") != NULL)
    {
      sprintf(request_buf, "Connection:close\r\n");
      is_connection_exist = 1;
    }
    else if(strstr(request_buf, "User-Agent") != NULL)
    {
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }
    else if(strstr(request_buf, "Host") != NULL)
    {
      is_host_exist = 1;
    }
  }
}

/*uri를 'hostname', 'port', 'path'로 파싱
  uri 형태 : 'http://hostname:port/path' 혹은 'http://hostname/path' (port는 optional)*/
int parse_uri(char *uri, char *hostname, char *port, char *path)
{
  //hot_name의 시작 위치 포인터: '//'가 있으면 //뒤(ptr+2)부터, 없으면 uri 처음부터
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  char *port_ptr = strchr(hostname_ptr, ':');   //port 시작 위치(없으면 NULL) 
  char *path_ptr = strchr(hostname_ptr, '/');   //path 시작 위치(없으면 NULL)
  strcpy(path, path_ptr);

  if(port_ptr)
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }
  else
  {
    if(is_local_test)
      strcpy(port, "80");
    else
      strcpy(port,"8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }

}

/*
 * serve_static - 정적 파일을 클라이언트에게 제공하는 함수
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize ,char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // HTTP 응답 헤더를 생성하여 클라이언트에게 전송
  get_filetype(filename, filetype); // 파일의 MIME 타입을 가져옴
  sprintf(buf, "%s 200 OK\r\n", http_version);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n", filesize);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));

  
  if(strcasecmp(method, "HEAD") == 0)
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // O_RDONLY - 읽기 전용으로 파일 열기
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리 매핑. PROT_READ - 메모리 보호 정책(읽기 가능) ,MAP_PRIVATE(해당 파일의 내용이 변경되는 것 방지)
  
  /* 11.9 malloc, rio_readn, rio_writen을 사용*/
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);                   // 파일디스크립터 닫고
  Rio_writen(fd, srcp, filesize); // 데이터 전송
  free(srcp);                           
  // Munmap(srcp, filesize);                                     // 메모리 매핑 해제
}

/*
 * get_filetype - 파일 이름에서 파일 유형 도출
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, "video/mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_dynamic - 클라이언트를 대신하여 CGI 프로그램을 실행
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "%s 200 OK\r\n", http_version);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  { /* 자식 프로세스 */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    // 자식 프로세스의 표준 출력을 클라이언트 소켓으로 리디렉션. CGI 프로그램의 출력이 클라이언트로 전송
    Dup2(fd, STDOUT_FILENO);

    // 실행할 프로그램의 경로, 인자로 전달할 명령행인수, 현재 환경 변수
    Execve(filename, emptylist, environ); // CGI 프로그램 실행
  }
  Wait(NULL); // 부모 프로세스는 자식 프로세스가 종료되기를 기다린다.
}
/* $end serve_dynamic */