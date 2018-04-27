#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <libgen.h>
#include <ctype.h>

int sockfd, newsockfd, portno;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
char* msg;
long fsize;
char* error;
int errno_temp;
int DEBUG = 1;

void report_error(const char* err)
{
  errno_temp = errno;
  fprintf(stderr, "Error due to: %s.\n", err);
  fprintf(stderr, "Error code: %d.\n", errno_temp);
  fprintf(stderr, "Error message: %s.\n", strerror(errno_temp));
  exit(EXIT_FAILURE);
}

int writeToClient(int sfd, char *data, int len)
{
  while (len > 0){
    int n = send(sfd, data, len, 0);
    if (n <= 0){
      fprintf(stderr, "The client was not written to\n");
      return 0;
    }
    data += n;
    len -= n;
  }
  return 1;
}

int writeFile(int sfd, int ffd, int len)
{
  int bytesSent=0;
  while (len > bytesSent){
    int n = sendfile(sfd, ffd, 0, len-bytesSent);
    if (n <= 0){
      fprintf(stderr, "The client was not written to\n");
      return 0;
    }
    bytesSent+=len;
  }
  return 1;
}

long fileSize(FILE *fp)
{
  if (fseek(fp, 0, SEEK_END) == -1) {
    error = "fseek";
    report_error(error);
  }

  long file_size = ftell(fp);
  if (file_size == -1) {
    error = "ftell";
    report_error(error);
  }
  rewind(fp);

  return file_size;
}

void setUpSockets()
{
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error = "socket";
    report_error(error);
  }
  memset((char *) &serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    error = "bind";
    report_error(error);
  }
}

char *findExtension(char *filename)
{
    char *dot = strrchr(filename, '.');

    if(dot==NULL) return "application/octet-stream";

    if(!dot || dot == filename) return "";

    if(strcmp(dot + 1, "jpeg") == 0)
    {
    	return "image/jpeg";
    }
    else if(strcmp(dot + 1, "jpg") == 0)
    {
    	return "image/jpg";
    }
    else if(strcmp(dot + 1, "gif") == 0)
    {
    	return "image/gif";
    }
    else if(strcmp(dot + 1, "txt") == 0)
    {
    	return "text/plain";
    }
    else if(strcmp(dot + 1, "png") == 0)
    {
    	return "image/png";
    }
    else if(strcmp(dot + 1, "htm") == 0 || strcmp(dot + 1, "html") == 0)
    {
    	return "text/html";
    }

    return "";
}

int loadFile(char* filename)
{
  if (strcmp(filename,"favicon.ico")==0) return 2;
  if(DEBUG) fprintf(stderr, "LOOKING FOR: %s\n", filename);
  FILE* f = fopen(filename,"r");
  if (!f) {
    return 0;
  }

  fsize = fileSize(f);
  if(DEBUG) fprintf(stderr, "size: %li\n", fsize);
  fclose(f);
  return 1;
}

char* getPath(char* request) {
  char* slash;
  char* Http;
  slash=strchr(request, '/');
  long path_start=slash-request;
  Http=strstr(request, "HTTP/1.1");
  long path_end=Http-request-1;
  char* path=malloc(path_end-path_start+1);
  strncpy(path, request+path_start, path_end-path_start);
  path[path_end-path_start]=0;
  return path;
}

char* spaceHandler(char *s)
{
    char* p=calloc(strlen(s),1);
    int i = 0;
    int j = 0;
    while (i!=strlen(s)) {
        if ((s[i]) == '%'){
            p[j]=' ';
            j++;
            i+=3;
        }
        else {
            p[j]=s[i];
            j++;
            i++;
        }
    }
    return p;
}

char* lostr(char *p)
{
    char *temp = strdup(p); // make a copy

    unsigned char *tptr = (unsigned char *)temp;
    while(*tptr) {
        *tptr = tolower(*tptr);
        tptr++;
    }

    return temp;
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
      fprintf(stderr, "ERROR, no port provided\n");
      exit(1);
  }
  portno = atoi(argv[1]);

  setUpSockets();

  if (listen(sockfd, 5) < 0) {
    error = "listen";
    report_error(error);
  }

  while (1) {
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0) {
      error = "accept";
      report_error(error);
    }

    char fromClientbuf[1024];
    int ret = recv(newsockfd, fromClientbuf, sizeof(fromClientbuf), 0);
    if (ret < 1){
      fprintf(stderr, "Could not read from Client\n");
      close(newsockfd);
      continue;
    }
    fromClientbuf[ret] = 0;
    char* validReq=strstr(fromClientbuf, "GET");
    if (!validReq) {
      fprintf(stderr, "Not a valid http request\n");
      continue;
    }
    char* icon=strstr(fromClientbuf, "favicon");
    if (icon) {
      continue;
    }
    printf("%s\n", fromClientbuf);

    char* path = getPath(fromClientbuf);
    if (DEBUG) fprintf(stderr, "%s\n", path);

    char* filename = basename(path);
    char* fileNameLower = lostr(filename);
    char* filename2 = spaceHandler(fileNameLower);
    if (DEBUG) fprintf(stderr, "FILENAME: %s\n", filename2);

    char* fileType = findExtension(filename2);
    if (DEBUG) fprintf(stderr, "TYPE: %s\n", fileType);

    int retLoad = loadFile(filename2);
    if (DEBUG) fprintf(stderr, "RET: %i\n", retLoad);

    char *data;
    if (retLoad==0) {
      data="HTTP/1.1 404 Not Found\r\nConnection: close\r\nServer: Web Server in C\r\nContent-Type: text/html\r\n";
      if (!writeToClient(newsockfd, data, strlen(data))) {
        close(newsockfd);
        continue;
      }
      char* err_msg = "<h1>ERROR 404 File Not Found</h1>";
      char Content_length[40];
      int n = sprintf(Content_length, "Content-length: %ld\r\n\r\n", strlen(err_msg));
      if (DEBUG) fprintf(stderr, "WRITING: %i\n", n);
      Content_length[n]=0;
      if (!writeToClient(newsockfd, Content_length, n)){
          close(newsockfd);
          continue;
      }

      if (!writeToClient(newsockfd, err_msg, strlen(err_msg))) {
        close(newsockfd);
        continue;
      }
      close(newsockfd);
    }

    else if (retLoad==2) {
      close(newsockfd);
    }

    else {
      char* headers = calloc(1, 1024);
      int n = sprintf(headers, "HTTP/1.1 200 OK\r\nConnection: close\r\nServer: Web Server in C\r\nContent-Type: %s\r\nContent-length: %ld\r\n\r\n", fileType, fsize);
      if (!writeToClient(newsockfd, headers, n)){
        close(newsockfd);
        continue;
      }

      int fd1 = open(filename2, O_RDONLY,0);

      if (!writeFile(newsockfd, fd1, fsize)){
          close(newsockfd);
          continue;
      }

      close(newsockfd);
    }
  }
  return 0;
}
