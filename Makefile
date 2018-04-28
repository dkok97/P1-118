CC=gcc
CPPFLAGS=-g -Wall
USERID=204818138

default: clean server

server:
	$(CC) -o webserver $^ $(CPPFLAGS) webserver.c

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball

tarball: clean
	tar -cvzf $(USERID).tar.gz webserver.c Makefile README report.pdf
