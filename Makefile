CC=gcc
CPPFLAGS=-g -Wall
USERID=123456789

default: clean server

server:
	$(CC) -o server $^ $(CPPFLAGS) server.c

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

# dist: tarball

# tarball: clean
# 	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
