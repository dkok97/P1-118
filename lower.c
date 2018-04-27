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
#include <libgen.h>
#include <ctype.h>
#include <dirent.h>

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

void lowerFiles()
{
    
    DIR *dp;
    struct dirent *ep;
    
    dp = opendir ("./");
    if (dp != NULL)
    {
        while (1)
        {
            ep=readdir(dp);
            if (ep==NULL) break;
            lostr(ep->d_name);
            fprintf(stderr, "%s\n", ep->d_name);
        }
        (void) closedir (dp);
    }
    else
        perror ("Couldn't open the directory");
    
}


int main(int argc, const char * argv[]) {
    char name[]="TeST.txt";
    char* newName = lostr(name);
    fprintf(stderr, "%s\n", newName);
    lowerFiles();
    return 0;
}

