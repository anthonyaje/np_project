#include <iostream>
#include <cstdio>
#include <cstdlib>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <errno.h>
#include <string>
#include <netinet/in.h>
//#include <arpa/inet.h>

#define PORTNUM 8867
#define MAXLINE 512

/*void str_echo(int sockfd){
	int n;
	char line[MAXLINE];
	
	for(;;){
		n = readline(sockfd, line, MAXLINE);
		if(n==0) return; //connection terminated
		else if(n<0) err_dump("str_echo: readline error\n");
		if(writen(sockfd,line,n) != n){
			err_dump("str_echo: written error");
		}
	}
}
*/
using namespace std;

void err_dump(string str){
	perror(str);
}

int print_ip(struct sockaddr_in* cli_addr);

int main (int argc, char* argv[]){
	int socketfd, newsocketfd, cli_addr_size, childpid;
	struct sockaddr_in cli_addr, serv_addr;
	
	int n;
	string buffer;	
	char* pname=argv[0];

	//open tcp socket
	if((socketfd=socket(AF_INET,SOCK_STREAM,0)) < 0)
		err_dump("server: can't open stream socket");

    //bind local address
    bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//serv_addr.sin_addr.s_addr = inet_addr("127.1.1.1");
	serv_addr.sin_port = htons(PORTNUM);

	if(bind(socketfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		err_dump("server: can't bind local address");

	listen(socketfd,5);
    
	for(;;){
		cli_addr_size = sizeof(cli_addr);
		newsocketfd = accept(socketfd,(struct sockaddr*) &cli_addr, &cli_addr_size)
;
		if(newsocketfd<0){
			perror("Error on accept");
			exit(1);
		}

		if((childpid=fork()) < 0) err_dump("server: accept error");   
		else if(childpid==0){ //child process
			close(socketfd);
			//process the request
			//str_echo(newsocketfd);
			cout<<"client connection accepted!"<<endl;
			cout<<"#### WELCOME NOTE ####"<<endl;

			//bzero(buffer, 256);
			//if(strcmp(buffer,"")) printf("buffer empty");
			while(1){
				n = read(newsocketfd, buffer, 255);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}
			//	if(strncmp(buffer,"exit",n-2)==0)
			//		break;
			//	printf("Here is the message: %s",buffer);
				
				//write a response to the client
				n = write(newsocketfd,"got it!\n",9);
				if(n<0){
					perror("Error writing to socket");
					exit(1);
				}
			
				bzero(buffer, 256);
			}
			exit(0);
		}
		close(newsocketfd); //parent process
	}

return 0;
}


/*
int print_ip(struct sockaddr_in* cli_addr){
	printf("%d.%d.%d.%d\n",
			(int)(cli_addr->sin_addr.s_addr&0xFF),
			(int)((cli_addr->sin_addr.s_addr&0xFF00)>>8),
			(int)((cli_addr->sin_addr.s_addr&0xFF0000)>>16),
			(int)((cli_addr->sin_addr.s_addr&0xFF000000)>>24));	
}
*/
