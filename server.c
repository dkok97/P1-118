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

int sockfd, newsockfd, portno;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
char* msg;
long fsize;
char* error;
int errno_temp;

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

void loadFile()
{
  FILE *f = fopen("index.html", "rb");
  if (!f) {
    error = "fopen";
    report_error(error);
  }

  fsize = fileSize(f);

  msg = (char*) malloc(fsize+1);
  if (!msg){
    error = "malloc";
    report_error(error);
  }

  if (fread(msg, fsize, 1, f) != 1){
    error = "fread";
    report_error(error);
  }
  msg[fsize] = 0;
  fclose(f);
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
    printf("%s\n", fromClientbuf);

    //parse the request
    //get the filename
    //search for files in directory
    //convert names to lowercase
    //use strstr maybe

    loadFile();

    int n;
    char *data;
    data="HTTP/1.1 200 OK\r\n";
    if (!writeToClient(newsockfd, data, sizeof(data))){
      close(newsockfd);
      continue;
    }

    data="Connection: close\r\n\r\n";
    if (!writeToClient(newsockfd, data, sizeof(data))){
      close(newsockfd);
      continue;
    }

    // const char *days[] = {"Sunday","Monday","Tuesday"};
    // char dateTime[100];
    // time_t t = time(NULL);
    // struct tm tm = *localtime(&t);
    // n = sprintf(dateTime, "Date: %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_wday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    // printf("%s\n", dateTime);

    data="Server: Web Server in C\r\n\r\n";
    if (!writeToClient(newsockfd, data, sizeof(data))){
      close(newsockfd);
      continue;
    }

    //other headers in response? Date, server, last-modified?

    char Content_length[40];
    n = sprintf(Content_length, "Content-length: %ld\r\n", fsize);
    if (!writeToClient(newsockfd, Content_length, n)){
        close(newsockfd);
        continue;
    }

    //char fileType[40];
    //n = sprintf(fileType, "Content-Type: %s\r\n", filetype);

    data="Content-Type: text/html\r\n";
    if (!writeToClient(newsockfd, data, sizeof(data))){
      close(newsockfd);
      continue;
    }

    if (!writeToClient(newsockfd, msg, fsize)){
        close(newsockfd);
        continue;
    }

    close(newsockfd);
  }
  return 0;
}
