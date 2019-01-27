all: webserver.c
	gcc -pthread -o webserver webserver.c

clean:
	$(RM) webserver
