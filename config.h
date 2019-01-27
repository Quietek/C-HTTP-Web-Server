#ifndef __CONFIG
#define __CONFIG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      
#include <strings.h>    
#include <unistd.h>      
#include <sys/socket.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <signal.h>

#define MAXLINE  4096 
#define MAXBUF   8192 
#define LISTENENQ  10  
#define FILENAME "ws.conf"

#endif

struct config
{
	char index[MAXBUF];
	char *extensions[20][2];
};

struct config get_config(){
	//variable declarations
	struct config configstruct;
	//open config file
	FILE *file = fopen(FILENAME, "r");
	//make sure file opened properly
	if (file != NULL){
		//declare variables
		char line[MAXBUF];
		char *Line1;
		char *Line2;
		int flag;
		int i;
		int linenum = 0;
		int delimpos;
		
		while(fgets(line, sizeof(line), file) != NULL){
			//initialize values for variables that need to reset on each line
			flag = 0;
			delimpos = 0;
			Line1 = malloc(sizeof(char) * strlen(line));
			Line2 = malloc(sizeof(char) * strlen(line));
			//loop through each character in the line until you find a newline character
			for(i = 0; line[i] != '\n'; i++){
				//if the flag has not been triggered, the flag indicates which part of the line split is being processed
				if(!flag){
					//as long as the line isn't being split at this character
					if (line[i] != ' '){
						Line1[i] = line[i];
					}
					//if the character is a space, the line gets split here. set the flag and the delimeter position value
					else{	
						flag = 1;
						delimpos = i+1;
					}
				}
				//if you're reading the second part of the line, read it into Line2
				else{
					Line2[i-delimpos] = line[i];
				}
			}
			//if its the first line, copy the value into the index portion of the struct	
			if (linenum == 0){
				memcpy(configstruct.index,Line2,strlen(Line2));
			}
			//if its not the first line, copy the values into the extensions portion of the struct
			else{
				configstruct.extensions[linenum-1][0] = malloc(strlen(Line1));
				memcpy(configstruct.extensions[linenum-1][0],Line1,strlen(Line1));
				configstruct.extensions[linenum-1][1] = malloc(strlen(Line2));
				memcpy(configstruct.extensions[linenum-1][1],Line2,strlen(Line2));
			}
			//increase the line number, since we are done with the line we read
			linenum++;
		}
		//output that the config file was read to the console and close the file
		printf("Config read successfully.\n");
	  	fclose(file);
	}
	//if the file failed to open, output that to the console
	else{
		printf("Error reading in config file, exiting...\n");
		fclose(file);
		close(0);
	}
	//return our struct we made
	return configstruct;
}

