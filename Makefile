SHELL=/bin/bash
CC=gcc
CFLAGS= -Wall -pedantic

backflags= -pthread -lrt

.PHONY: test
.PHONY: clean

define script =
S=8
k=5
m=20
if ! [ -d output ]; then
mkdir output
fi
(./supervisor $$S > output/supervisor ) &
spid=$$!
sleep 2
for (( i=1; i<=20; i+=2 ));
do 
( ./client $$S $$k $$m > output/client$$i ) &
( ./client $$S $$k $$m > output/client$$(($$i+1))) &
sleep 1
done
for (( j=1; j<=6; j++ ));
do
sleep 10
kill -SIGINT $$spid
done
sleep 0.1
kill -SIGINT $$spid
sleep 1
./misura.sh
endef

define rmscript =
if [ -r supervisor ] ; then rm supervisor else echo "supervisor assente" fi
endef

all: client server supervisor

client: client.o support.o
	$(CC) client.o support.o -o client $(backflags)

server: server.o support.o
	$(CC) server.o support.o -o server $(backflags)

supervisor: supervisor.o support.o
	$(CC) supervisor.o support.o -o supervisor $(backflags)

client.o: client.c support.h
	$(CC) $(CFLAGS) -c client.c -o client.o $(backflags)

server.o: server.c support.h
	$(CC) $(CFLAGS) -c server.c -o server.o $(backflags)

supervisor.o: supervisor.c support.h
	$(CC) $(CFLAGS) -c supervisor.c -o supervisor.o $(backflags)

support.o: support.c support.h
	$(CC) $(CFLAGS) -c support.c -o support.o

test: ; @$(script)

clean:
	@rm supervisor server client supervisor.o server.o client.o support.o
	@if [ -d output ] ; then
	@rm -r output
	@fi


.ONESHELL: test
