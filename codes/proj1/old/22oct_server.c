#include <stdio.h>
#include <stdlib.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <errno.h>
#include <string.h>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include <unistd.h>


#define PORTNUM 8867
#define BUFFER_SIZE 256

void err_dump(char* str){
	perror(str);
}

int print_ip(struct sockaddr_in* cli_addr);

void parse_command(char* command, int sockfd);

int main (int argc, char* argv[]){
	int socketfd, newsocketfd, cli_addr_size, childpid;
	struct sockaddr_in cli_addr, serv_addr;
	
	int n;
	char buffer[BUFFER_SIZE];	
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
			printf("client connection accepted!\n");
			n = write(newsocketfd,"#### WELCOME NOTE from server  ####\n",37);
			if(n<0){
				perror("Error reading from socket");
				exit(1);
			}

			bzero(buffer, BUFFER_SIZE);
			while(1){
				//write a response to the client
				n = write(newsocketfd,"%",1);
				if(n<0){
					perror("Error writing to socket");
					exit(1);
				}

				n = read(newsocketfd, buffer, 255);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}

				if(strncmp(buffer,"exit",n-2)==0)
					break;

				//printf("Here is the message: %s",buffer);
				parse_command(buffer, newsocketfd);
			
				printf("### parse done ####\n");
				bzero(buffer, BUFFER_SIZE);
			}
			exit(0);
		}
		close(newsocketfd); //parent process
	}

return 0;
}

int print_ip(struct sockaddr_in* cli_addr){
	printf("%d.%d.%d.%d\n",
			(int)(cli_addr->sin_addr.s_addr&0xFF),
			(int)((cli_addr->sin_addr.s_addr&0xFF00)>>8),
			(int)((cli_addr->sin_addr.s_addr&0xFF0000)>>16),
			(int)((cli_addr->sin_addr.s_addr&0xFF000000)>>24));	
}

void parse_command(char* command,int sockfd){
	char* line;
//    const char delim[3]={' ',"\r","\n"};
	line = strtok(command,"\r\n");
	int status;
	pid_t pid;
	int pipefd[2];
	pipe(pipefd);
	while(line != NULL){
		printf("line: %s#\n",line);
		pid = fork();
		if(pid==0){ //child
			char* token;
			close(pipefd[0]);
			dup2(pipefd[1],1);
			dup2(pipefd[1],2);
			close(pipefd[1]);

			token = strtok(line," \r\n");
			while(token != NULL){	
				printf("token: #%s#\n",token);
				if(strcmp(token,"ls")==0){
					printf("here is ls\n");
					execlp(token,token,0,0);
					//_exit(EXIT_FAILURE);

				}else if(strcmp(token,"cat")==0){
					printf("here is cat\n");
					char* args[2];
					args[0] = token;
					args[1] = strtok(NULL," \r\n");
					execlp(args[0],args[0],args[1],NULL);
					//_exit(EXIT_FAILURE);

				}else if(strcmp(token,"printenv")==0){
					printf("here is printenv");
					/*char* args[2];
					args[0] = token;
					args[1] = strtok(NULL," \r\n");
					*/
					//execlp(args[0],args[0],args[1],NULL);
					//_exit(EXIT_FAILURE);

				}else if(strcmp(token,"setenv")==0){
					perror("here is setenv");
					/*char* args[3];
					args[0] = token;
					args[1] = strtok(NULL," \r\n");
					args[2] = strtok(NULL," \r\n");
					execlp(args[0],args[0],args[1],args[2],NULL);
					*/
					//execl("export","export","PATH=bin");
					//_exit(EXIT_FAILURE);
				
				}else{
					printf("Unknown Command: %s\n",token);
					_exit(EXIT_FAILURE);
				}	
				token = strtok(NULL," \r\n");
			}
			return;
	
		}else if(pid<0){ 
			perror("parse command: fork failed");
			status = -1;
		}else{ //parent
			if(waitpid(pid,&status,0) != pid)
				status = -1;

			char buffer[1024];
			bzero(buffer,1024);

			close(pipefd[1]);
			while(read(pipefd[0],buffer, sizeof(buffer)) != 0){
				//TODO send back to client
				printf("%s",buffer);

				//write(newsocketfd,buffer,sizeof(buffer));
			}
			
			bzero(buffer,1024);
			line = strtok(NULL,"\r\n");
		}
	}
}
