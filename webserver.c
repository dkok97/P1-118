#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <dirent.h>

int sockfd, newsockfd, portno;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
char* msg;
long fsize;
char* error;
int errno_temp;
int DEBUG = 1;
struct stat path_stat;

//catches error and exists code
void report_error(const char* err)
{
  errno_temp = errno;
  fprintf(stderr, "Error due to: %s.\n", err);
  fprintf(stderr, "Error code: %d.\n", errno_temp);
  fprintf(stderr, "Error message: %s.\n", strerror(errno_temp));
  exit(EXIT_FAILURE);
}

//using send to send content from string data to to client opened in socket sfd
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

//using sendfile to send content from file opened in socket ffd to client opened in socket sfd
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

//returns file size by seeking to end, reading file pointer value, and restoring fp to beginning of file
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

//boilerplate socket code
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

//matches file extension to MIME type. If not extension, return application/octet-stream
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

//looks for / in request and gets path from / to HTTP/1.1
char* getPath(char* request) {
  char* slash;
  char* Http;
  slash=strchr(request, '/');                                       //returns first occurence of '/'
  long path_start=slash-request;
  Http=strstr(request, "HTTP/1.1");                                 //returns first occurence of "HTTP/1.1"
  long path_end=Http-request-1;
  char* path=malloc(path_end-path_start+1);
  strncpy(path, request+path_start, path_end-path_start);
  path[path_end-path_start]=0;
  return path;
}

//replace %20 in string with space
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

//convert string to lower case
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

char* lowerFiles(char* filename)
{
    DIR *dp;
    struct dirent *ep;

    dp = opendir ("./");
    if (dp != NULL)
    {
        while (1)                                                     //opens dir and loops through all files, comparing them
        {                                                             //to the file requested in a case-insensitive manner
            ep=readdir(dp);
            if (ep==NULL) break;
            if (strcmp(filename, lostr(ep->d_name))==0) {
              stat(ep->d_name, &path_stat);
              if(!S_ISREG(path_stat.st_mode)) return "";              //if file is not a  regular file, pring 404
              return ep->d_name;
              break;
            }
        }
        (void) closedir (dp);
    }
    else {
      error = "opendir";
      report_error(error);
    }
    return "";
}

int loadFile(char* filename)
{
  if (strcmp(filename,"favicon.ico")==0) return 2;
  if(DEBUG) fprintf(stderr, "LOOKING FOR: %s\n", filename);
  char* isFile = lowerFiles(filename);                                //checks in dir for files in a case-insensitive manner
  if (strcmp(isFile,"")==0) return 0;
  FILE* f = fopen(isFile,"r");
  if (!f) {
    return 0;
  }

  fsize = fileSize(f);                                                //returns file size
  if(DEBUG) fprintf(stderr, "size: %li\n", fsize);
  fclose(f);
  return 1;
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
      fprintf(stderr, "ERROR, no port provided\n");
      exit(1);
  }
  portno = atoi(argv[1]);   //first cli arguement is port number

  //sets up sockets with port number
  setUpSockets();

  //start listening on portno
  if (listen(sockfd, 5) < 0) {
    error = "listen";
    report_error(error);
  }

  while (1) {
    //accept and recieve request from client
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
    char* validReq=strstr(fromClientbuf, "GET");                    //checking if the reques is a valid GET request
    if (!validReq) {
      fprintf(stderr, "Not a valid http request\n");
      continue;
    }
    char* icon=strstr(fromClientbuf, "favicon");                    //ignoring request for favicon
    if (icon) {
      continue;
    }
    printf("%s\n", fromClientbuf);

    char* path = getPath(fromClientbuf);                            //getting path to file from the request
    if (DEBUG) fprintf(stderr, "%s\n", path);

    char* filename = basename(path);                                //name of file at end of path
    char* fileNameLower = lostr(filename);                          //converting path to lowercase
    char* filename2 = spaceHandler(fileNameLower);                  //replaces %20 in name with space
    if (DEBUG) fprintf(stderr, "FILENAME: %s\n", filename2);

    char* fileType = findExtension(filename2);                      //gets extension from filename
    if (DEBUG) fprintf(stderr, "TYPE: %s\n", fileType);

    int retLoad = loadFile(filename2);                              //loads file and calculates filesize
    if (DEBUG) fprintf(stderr, "RET: %i\n", retLoad);

    char *data;

    //If file is not found, or is not a regular file, send back ERROR 404
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

    //Ignore favicon request
    else if (retLoad==2) {
      close(newsockfd);
    }

    //If file is found, and is a regular file, send back headers and file content
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
