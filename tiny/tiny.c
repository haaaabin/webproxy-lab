/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void echo(int connfd);

/* 11.6C http_version */
char *http_version;

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);

    // 클라이언트와 통신할 새로운 소켓의 파일 디스크립터 반환
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // 클라이언트의 주소 정보를 호스트 이름과 포트번호로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // 연결된 클라이언트와의 통신 처리
    doit(connfd);
    //echo(connfd);
    Close(connfd);
  }
}

void echo(int connfd)
{
  size_t n;
  char buf[MAXLINE];
  rio_t rio;
  Rio_readinitb(&rio, connfd);
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
  { // line:netp:echo:eof
    printf("server received %d bytes\n", (int)n);
    Rio_writen(connfd, buf, n);
  }
}

/* 한 개의 HTTP 트랜잭션을 처리한다.*/
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);           // 데이터를 읽을 리오 버퍼 초기화
  Rio_readlineb(&rio, buf, MAXLINE); // 서버에서 응답을 읽어들임
  printf("Request headers:\n");
  printf("%s", buf); // 헤더 GET / 1.0 정보
  sscanf(buf, "%s %s %s", method, uri, version);

  // strcasecmp() 대소문자 무시하고 두 개의 문자열을 비교하는 함수, 같으면 0 다르면 1 반환
  if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") !=0)
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  http_version = version;

  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static)
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size,method);
  }
  else
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs,method);
  }
  
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

/* HTTP 요청 헤더를 읽어오는 함수 . 주어진 rio_t 타입의 버퍼에서 HTTP 요청 헤더를 읽음*/
/* HTTP 요청이나 응답을 처리할 때 헤더 정보를 읽어와야 할 때 사용
  헤더 정보를 읽어오는 동안 빈 줄을 만나면 헤더의 끝을 나타내므로 그 이후의 데이터는 본문으로 처리*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // 버퍼에서 한 줄씩 데이터 읽어옴
  while (strcmp(buf, "\r\n"))
  { // 빈 줄을 만날 때까지 반복 (캐리지 리턴(carriage return)과 줄 바꿈(newline))
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*주어진 URI를 파싱하여 정적, 동적 컨텐츠를 구분하고 해당하는 파일 이름과 CGI 인자 추출*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* Static content */
  // strstr - 특정 부분 문자열 찾음
  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");             // CGI 인자 스트링 지움
    strcpy(filename, ".");           // filename에 현재 디렉토리를 나타내는 .문자열 복사
    strcat(filename, uri);           // filename에 URI 이어붙이기
    if (uri[strlen(uri) - 1] == '/') // URI가 /로 끝난다면
      strcat(filename, "home.html"); // filename에 home.html 추가
    return -1;
  }
  /* Dynamic content */
  else
  {
    // 모든 CGI 인자들을 추출
    ptr = index(uri, '?'); //? 위치 찾음
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // CGI 인자를 'cgiargs'문자열에 복사. ? 다음문자부터
      *ptr = '\0';              // 널 종단 문자 삽입.
    }
    else
      strcpy(cgiargs, "");

    // 나머지 URI 부분은 상대 리눅스 파일 이름으로 변환
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
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