// Based on ClientServer_Example code
// Simple server using TCP
// Port number is the first command line argument
// The server runs forever, using fork to handle
// each incoming connection in a child process

#include <stdio.h>
#include <sys/types.h> // Data types used in socket.h and netinet/in.h
#include <sys/socket.h> // Struct definitions for sockets
#include <netinet/in.h> // Constants and structs for IP addresses
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h> // waitpid()
#include <signal.h> // Signal name macros and kill()
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <iostream> // cout (for debugging)
#include <fstream> // ifstream
#include <sstream> // stringstream
#include <algorithm> // transform
#include <sys/stat.h> // stat for file time modification info

using namespace std;

void sigchld_handler(int s)
{
    (void)s;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Print error message and then exit
void error(string msg)
{
    perror(msg.c_str());
    exit(1);
}

// Main function for processing and responding to TCP requests
// Each child process calls this function once to process the request it's handling
void respondToClient(int sockfd);

int main(int argc, char** argv)
{
    int sockfd, newsockfd, portno, pid;
    socklen_t cli_len;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;
    
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (::bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    listen(sockfd, 5);
    
    cli_len = sizeof(cli_addr);
    
    // Kill zombie processes
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1)
        error("sigaction");
    
    while (true)
    {
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);
        
        if (newsockfd < 0)
            error("ERROR on accept");
        
        // Fork to handle incoming requests
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");
        
        // Child process: close old socket, handle request, close new socket
        if (pid == 0)
        {
            close(sockfd);
            respondToClient(newsockfd);
            close(newsockfd);
            exit(0);
        }
        else // Parent closes new socket fd
            close(newsockfd);
    }
    
    return 0;
}

const string response_base = "HTTP/1.1 200 OK\r\nConnection: close\r\nServer: webserver/0.0.1\r\n"; // If the request is well formed, this is the base of the response
const string failure_msg = "HTTP/1.1  Not Found\r\nConnection: close\r\nServer: webserver/0.0.1\r\nContent-Type: text/html\r\n";
const string failure_page = "<h1>404 Page Not Found</h1>"; // If a file can't be found, this is the response
const string default_mime = "application/octet-stream"; // Default MIME if a mapping does not exist
const string date_format = "%a, %d %b %Y %T GMT";
const unordered_map<string, string> MIMEs = // Mapping of file extensions to MIME types
{
    { ".html", "text/html" },
    { ".gif", "image/gif" },
    { ".jpg", "image/jpeg" },
    { ".jpeg", "image/jpeg" }
};

void respondToClient(int sockfd)
{
    int n;
    char buffer[512] = {0}; // TODO: it is not responsible to assume that the maximum size of an HTTP request is 512 bytes
    string request;
    
    n = read(sockfd, buffer, 511);
    if (n < 0)
        error("ERROR reading from socket");
    
    request.append(buffer, n);
    
    size_t start = 0;
    size_t end = 0;
    
    string method;
    string uri;
    string version;
    
    // Get method, URI, and version info
    end = request.find(' ', start);
    if (end == string::npos)
        return;
    method = request.substr(start, end - start);
    start = end + 1;
    end = request.find(' ', start);
    if (end == string::npos)
        return;
    uri = request.substr(start, end - start);
    start = end + 1;
    end = request.find("\r\n", start);
    if (end == string::npos)
        return;
    version = request.substr(start, end - start);
    start = end + 2;
    
    
    cout << "Method: " << method << endl;
    cout << "URI: " << uri << endl;
    cout << "Version: " << version << endl;
    
    unordered_map<string, string> headers;
    // Store headers into the map declared above
    while (true)
    {
        string key, value;
        
        end = request.find(": ", start);
        if (end == string::npos)
            break;
        key = request.substr(start, end - start);
        start = end + 2;
        end = request.find("\r\n", start);
        if (end == string::npos)
            break;
        value = request.substr(start, end - start);
        start = end + 2;
        
        cout << "Key: " << key << " Value: " << value << endl;
    }
    
    string response;
    string content;
    string path = "." + uri;
    struct tm* timeinfo;
    
    struct stat attr;
    int res = stat(path.c_str(), &attr);
    ifstream ifs(path);
    cout << uri << endl;
    if (ifs.fail() || (res == 0 && S_ISDIR(attr.st_mode))) // If the file couldn't be opened or is a directory, use the failure response (404 Not Found)
    {
        response = failure_msg;
        content = failure_page;
    }
    else // Finish forming the HTTP response
    {
        stringstream buf;
        buf << ifs.rdbuf();
        content = buf.str();
        
        cout << "buf: " << &buf << "\n";
        cout << "content: " << content << "\n";
        
        string MIME;
        size_t pos = uri.rfind('.');
        if (pos != string::npos) // Retrieve mapping and add Content-Type header
        {
            string extension = uri.substr(pos);
            transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            auto it = MIMEs.find(extension);
            if (it != MIMEs.end())
                MIME = it->second;
            else
                MIME = default_mime;
        }
        else // Use default mapping
            MIME = default_mime;
        response = response_base + "Content-Type: " + MIME + "\r\n";
        
        // Last-Modified header
        if (res == 0)
        {
            timeinfo = gmtime(&attr.st_mtime);
            if (strftime(buffer, 512, date_format.c_str(), timeinfo) != 0)
                response += "Last-Modified: " + string(buffer) + "\r\n";
        }
    }
    
    // Date header
    time_t curtime;
    time(&curtime);
    timeinfo = gmtime(&curtime);
    if (strftime(buffer, 512, date_format.c_str(), timeinfo) != 0)
        response += "Date: " + string(buffer) + "\r\n";
    
    // Add content
    response += "Content-Length: " + to_string(content.length()) + "\r\n\r\n" + content;
    
    // Send response
    n = write(sockfd, response.c_str(), response.length());
    if (n < 0)
        error ("ERROR writing to socket");
}

