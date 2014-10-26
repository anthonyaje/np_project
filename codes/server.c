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
#define MAX_LINE_CHAR 100

void err_dump(char* str){
	perror(str);
}

//PROTOTYPE
int print_ip(struct sockaddr_in* cli_addr);
void parse_command(char* command, int sockfd);
int num_of_pipe(char* line);
void process1(char* command, int* arr); //move the pipe number from command into arr
void exec_comm(char* token);

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
	int i,status,n_pipe,pcount;
	pid_t pid;
	int** pipefd;
	char* instruct;
	int* n_arr;
	int* dirty_bit;    

	n_pipe = num_of_pipe(command);
	pipefd = malloc(n_pipe * sizeof(int*));
	for(i=0;i<n_pipe;i++){
		pipefd[i] = malloc(2 * sizeof(int));
		pipe(pipefd[i]);
	}

	n_arr = malloc((n_pipe)*sizeof(int));
	dirty_bit = (int*) calloc((n_pipe),sizeof(int));  // 1:written 0:clean
	pcount=0; 					//count the order of command
	
	process1(command, n_arr); 	//process the pipe n into array
	n_arr[n_pipe-1] = 1; 		//last pipe is always 1	

	instruct = strtok(command,"|\r\n");
	while(instruct != NULL){
	printf("instruct %d: %s\n",pcount,instruct);
		pid = fork();
		if(pid==0){ //child
			char* token;
			int temp_pipe[2];
			pipe(temp_pipe);

			if(pcount==0){ 			//first instruction
				dirty_bit[pcount]=1;
		   		close(pipefd[pcount][0]); 
		   		dup2(pipefd[pcount][1],1);
		   		dup2(pipefd[pcount][1],2);
		   		close(pipefd[pcount][1]); 
			}
			else{
				close(pipefd[pcount][0]); 		//close reading end in the child
				//prev pipe
				if(dirty_bit[pcount-1]==0){ 				//the pipe is clean
					//FIXME
					// no need to read from pipe, stdin redirect to??
					dup2(pipefd[pcount-1][0],0);	//direct the stdin from previous pipefd
				}
				else{							//the pipe is dirty
					//TODO: dirty pipe need to be process: copy, concatenate, writeback
					dup2(pipefd[pcount-1][0],0);	//direct the stdin from previous pipefd
											
				}
				//next pipe
				if(dirty_bit[pcount + n_arr[pcount] - 1] == 0){
		   			dup2(pipefd[pcount + n_arr[pcount] - 1][1],1);		//direct the stdout to the pipefd[pcount]
		   			dup2(pipefd[pcount + n_arr[pcount] - 1][1],2);		//direct the stderr to the pipefd[pcout]
				}
				else{
					//TODO: dirty pipe need to be process: copy, concatenate, writeback
					//close(temp_pipe[0]);
					dup2(temp_pipe[1],1);
					dup2(temp_pipe[1],2);
					close(temp_pipe[1]);
				}
		   		close(pipefd[pcount][1]);		//wont need this write description 
				
				/*
				close(pipefd[pcount][0]); 		//close reading end in the child
				dup2(pipefd[pcount-1][0],0);	//direct the stdin from previous pipefd
		   		dup2(pipefd[pcount + n_arr[pcount] - 1][1],1);		//direct the stdout to the pipefd[pcount]
		   		dup2(pipefd[pcount + n_arr[pcount] - 1][1],2);		//direct the stderr to the pipefd[pcout]
		   		close(pipefd[pcount][1]);		//wont need this write description 
				*/
			}
		
			token = strtok(instruct," \r\n");
			while(token != NULL){				//FIXME "if" should be enough
				printf("token: ##%s##\n",token);
				exec_comm(token);				//compare the string and execute it
				token = strtok(NULL," \r\n");
			}
			
			if((pcount!=0) && (dirty_bit[pcount + n_arr[pcount] - 1]!=0)){
				//new pipe, write stdout.. read old data to buffer and append with the new datam write back to the old pipe			
				char t1_buff[MAX_LINE_CHAR]={};
				char t2_buff[MAX_LINE_CHAR]={};
				read(pipefd[pcount + n_arr[pcount] - 1][0], t1_buff, MAX_LINE_CHAR);
				read(temp_pipe[0], t2_buff, MAX_LINE_CHAR);
				close(temp_pipe[0]);
				strcat(t1_buff,t2_buff);
				write(pipefd[pcount + n_arr[pcount] - 1][0], t1_buff, MAX_LINE_CHAR);
			}	

			_exit(EXIT_FAILURE);
		}
		else if(pid<0){ 
			perror("parse command: fork failed");
			status = -1;
		}
		else{ //parent
			printf("parent waiting\n");
			waitpid(pid,&status,0);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
			printf("parent done waiting\n");

			instruct = strtok(NULL,"|\r\n");
			close(pipefd[pcount][1]);
			pcount++; 							//instruction counter
			//line = strtok(NULL,"\r\n");
		}
	}
	
	char buffer[1024];
	bzero(buffer,1024);
	while(read(pipefd[pcount-1][0],buffer, sizeof(buffer)) != 0){
		printf("%s",buffer);
		write(sockfd,buffer,sizeof(buffer));
	}
	bzero(buffer,1024);
	printf("send back done\n");

}

int num_of_pipe(char* line){
	int i,n;
	n=0;
	for(i=0; i<strlen(line); i++){
		if(line[i]=='|') n++;		
	}

return n+1;
}

void process1(char* command, int* arr){
	char* it=command;
	int i,j;
	j=0;
	for(i=0; i<strlen(command); i++){
		if(*(it+i) == '|'){
			if( *(it+i+1) == ' ')
				arr[j++] = 1;
			else{
				arr[j++] = atoi(it+i+1);
				*(it+i+1) = ' ';
			}
		}
	}
}

void exec_comm(char* token){
	if(strcmp(token,"ls")==0){
		execlp(token,token,0,0);

	}else if(strcmp(token,"cat")==0){
		char* args[2];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		execlp(args[0],args[0],args[1],NULL);

	}else if(strcmp(token,"printenv")==0){
		char* args[2];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		execlp(args[0],args[0],args[1],NULL);

	}else if(strcmp(token,"setenv")==0){
		char* args[3];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		args[2] = strtok(NULL," \r\n");
		//printf("arg012 %s %s %s\n",args[0],args[1],args[2]);
		execlp(args[0],args[0],args[1],args[2],NULL);

	}else if(strcmp(token,"number")==0){
		char* args[2];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		//execlp(args[0],args[0],args[1],NULL);
		execl("./number",args[0],args[1],NULL);
	}else if(strcmp(token,"noop")==0){
		execlp(token,token,NULL);
	}else if(strcmp(token,"removetag")==0){
		char* args[2];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		//execlp(args[0],args[0],args[1],NULL);
		execl("./removetag",args[0],args[1],NULL);
	}else if(strcmp(token,"removetag0")==0){
		execlp(token,token,NULL);
	}
	else{
		printf("Unknown Command: %s\n",token);
	}

}	
