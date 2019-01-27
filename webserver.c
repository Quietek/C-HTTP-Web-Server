#include "config.h"

struct config configuration;

//function prototypes

int connection(void * vargp);
void error(char *msg);
void sigHandler(int signum);
void *thread(void * vargp);
void *HTTP_err_send(void * vargp, int StatusNum);
void *HTTP_send(void * vargp, char *file, char *extension);

//signal handler exit flag

int quit = 0;

int main(int argc, char **argv)
{
	//read in information from config file
	configuration = get_config(FILENAME);

	//declare variables
	int sockfd;
	int optval;
	int portno;
	int n; 
	int *listenfd;
	int clientlen = sizeof(struct sockaddr_in);
	char buf[MAXLINE];
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	pthread_t thr;


	//debug message to let user know the proper usage of program
	if (argc != 2){
		printf ("Usage: %s <port>", argv[0]);
		exit(1);
	}
	//convert port from command line argument to integer for use on connection
	portno = atoi(argv[1]);

	//declare socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		error("ERROR opening socket");
	}
	
	/* setsockopt: Handy debugging trick that lets 
   	* us rerun the server immediately after we kill it; 
   	* otherwise we have to wait about 20 secs. 
   	* Eliminates "ERROR on binding: Address already in use" error. 
   	*/
	optval = 1;
  	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
  	
  	
	/*
   	* build the server's Internet address
   	*/  	
   	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr= htonl(INADDR_ANY);
	serveraddr.sin_port = htons(portno);
	
	/* 
   	* bind: associate the parent socket with a port 
   	*/
	bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	
	//make accept non-blocking with fcntl
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	
	//listen on the socket
	listen(sockfd, LISTENENQ);

	//print status information
	printf("Server online...waiting for connections\n");
	printf("Press CTRL + C to quit server\n");

	//function call for CTRL+C
	signal(SIGINT, sigHandler);

	//while loop to accept connections on the socket, when quit flag set it exits.
	while(!quit){
		listenfd = malloc(sizeof(clientaddr));
		*listenfd = accept(sockfd, (struct sockaddr *) &clientaddr, &clientlen);
		if (*listenfd == -1){
			if (errno == EWOULDBLOCK) {
				//keep the socket from blocking, but do nothing and continue the loop if no connection is present
			}
			else {
				error("ERROR when accepting connection");
			}
		}
		else{
		//spawn thread for new request
			pthread_create(&thr, NULL, thread, listenfd);
		}
	}
	//if the quit flag is set via a signal, close the socket and exit
	if (quit){
		close(*listenfd);
	}
	return 0;
}

/*
 * HTTP_err_send - function to send HTTP response if the response is anything other than 200
 */
void *HTTP_err_send(void * vargp, int StatusNum){
	//convert our socket to proper formatting
	int listenfd = *((int *)vargp);
	//declare variables
	int len;
	int n;
	char str[MAXLINE];
	bzero(str, MAXLINE);
	//check the HTTP response number and copy the correct response into our string declared earlier
	if (StatusNum == 404){
		strcpy(str,"HTTP/1.1 404 Not Found\r\nServer : Web Server in C\r\n\r\n<html><head><title>404 Not Found</title></head><body><p>404 Not Found: The requested resource could not be found</p></body></html>");
	}
	else if (StatusNum == 415) {
		strcpy(str,"HTTP/1.1 415 Unsupported Media Type\r\nServer : Web Server in C\r\n\r\n<html><head><title>415 Unsupported Media Type</head></title><body><p>415 Unsupported Media Type</p></body></html>");
	}
	else if (StatusNum == 500){
		strcpy(str,"HTTP/1.1 500 Internal Server Error\r\nServer : Web Server in C\r\n\r\n<html><head><title>500 Internal Server Error</title></head><body><p>500 Internal Server Error</p></body></html>");
	}
	//find the length of the string containing the response
	len = strlen(str);
	//send our HTTP response
	n = send(listenfd, str, len, 0);
	//error handling if response failed to send
	if (n < 0){
		printf("Error sending HTTP response %d\n",StatusNum);
	}
	else{
		printf("Sent HTTP response %d\n", StatusNum);
	}
}

void *HTTP_send(void * vargp, char *file, char *extension) {
	//variable declarations
	char filelength[64];
	char buf[MAXBUF];
	struct stat file_stat;
	int fd;
	int *listenfd;
	int block;
	int length;
	listenfd = malloc(sizeof(*vargp));
 	*listenfd = *((int *)vargp);
 	//zero out buffer
 	bzero(buf, MAXBUF);
 	//open the requested file
 	fd = open(file, O_RDONLY, 0);
 	//output the status of the file, 0 opened successfully, -1 failed to open
	printf("File Status: %d\n",fstat(fd, &file_stat));
	//if the file opened properly
	if (fstat(fd, &file_stat) >= 0){
		sprintf(filelength,"%ld***\n", file_stat.st_size);
		//begin sending HTTP request information
		sprintf(buf,"HTTP/1.1 200 OK\r\nContent-Type: %s\nContent-Length: %s\n",extension,filelength);
		send(*listenfd, buf, strlen(buf),0);
						
		//convert file size into integer
		length = atoi(filelength);
						
		//create new output buffer
		bzero(buf, MAXBUF);
		//read file into buffer-> send  buffer-> zero out buffer -> repeat until EOF
		while((block = read(fd, buf, MAXBUF)) > 0){
			if(send(*listenfd, buf, block, 0) < 0){
				printf("\n");
			}
			bzero(buf, MAXBUF);
		}
		//close the file
		close(fd);
		printf("Sent HTTP response 200\n");
	}
	//if the file failed to open, send a 404 error
	else {
		HTTP_err_send(listenfd, 404);
	}
}
/*
 * thread - function to spawn a new thread, then execute the connection function
 */ 
void * thread(void * vargp)
{
	//convert vargp to the proper format for our listening socket file descriptor
	int *listenfd;
	listenfd = malloc(sizeof(*vargp));
	*listenfd = *((int *)vargp);
	//detatch thread, allowing for proper resource allocation
	pthread_detach(pthread_self());
	//begin connection function in new thread
	connection(listenfd);
	//close the socket
	close(*listenfd);
}
/*
 * connection - function spawned by thread, handles actual HTTP requests and connections
 */
int connection(void * vargp) {
	//variable declarations
	char resource[500];
	char *ptr;
	char buf[MAXLINE];
 	int *listenfd; 
 	int n = 0;
 	listenfd = malloc(sizeof(*vargp));
 	*listenfd = *((int *)vargp);
 	//error handling for if packet receive fails
 	n = recv(*listenfd, buf, MAXLINE, 0);
 	if (n < 0){
  		printf("Failed to receive message from socket\n");
	}
	printf("\n\n%s\n\n",buf)
  	//find HTTP/ substring in buf
 	ptr = strstr(buf, " HTTP/");
 	
 	//error handling if HTTP request not present
 	if (!ptr) {
  		HTTP_err_send(listenfd, 500);
 	} 
 	//If HTTP request present
 	else {
		//zero out ptr to be used again
		*ptr = 0;
		ptr = NULL;
		char cwd[MAXLINE];
		//error handling if not a GET request

		//if GET is successfully parsed from the buffer
		if (!strncmp(buf, "GET ", 4)){  
			ptr = buf + 4;
			//check what they're reqeusting, if its the root directory return index.html
			if (ptr[strlen(ptr) - 1] == '/') {
				strcat(ptr, configuration.index);
			}
			//copy the root directory into the resource, then append the ptr onto it, creating full path to index.html
			getcwd(cwd, sizeof(cwd));
			strcpy(resource, cwd);
			strcat(resource,"/www");
			strcat(resource, ptr);
			//find the last occurance of . in resource, to pull file extension
			char* s = strrchr(resource, '.');
			int i;
			int flag = 0;
			for (i = 0; configuration.extensions[i] != NULL; i++) {
				if (!strcmp(s, configuration.extensions[i][0])) {
					//if the file format is supported in the extensions, flag to prevent 415 error
					flag = 1;
					printf("Requested File: %s\n",resource);
					//send the requested file over HTTP
					HTTP_send(listenfd, resource, configuration.extensions[i][0]);
					break;
				}
			}
			//if the requested file format was not found in the config
			if (flag == 0) {
				HTTP_err_send(listenfd, 415);
			}
		//close the socket
		close(*listenfd);
		}
		else {
			HTTP_err_send(listenfd, 500);
		} 
	}
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(1);
}
/*
 * sigHandler - signal handler function to gracefully exit server 
 */
void sigHandler(int signum){
	printf("Caught signal %d, closing listening socket and server...\n",signum);
	quit = 1;
}
