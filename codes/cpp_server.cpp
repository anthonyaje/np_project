#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
//#include <sys/socket.h>
//#include <errno.h>
#include <cstring>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

#define PORTNUM 8867
#define BUFFER_SIZE 256

using namespace std;

//GLOBAL
vector<pair<int*,int> > pipeVec;

void err_dump(char* str){
	perror(str);
}
//PROTOTYPE
int print_ip(struct sockaddr_in* cli_addr);
void parse_command(char* command, int sockfd);
int num_of_pipe(char* line);
void process1(char* command, int* arr); //move the pipe number from command into arr
void exec_comm(char* token);
void check_update_vec();
void remove_zero_vec();

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
		newsocketfd = accept(socketfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size);
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
				//TODO if the buffer is huge
				n = read(newsocketfd, buffer, 255);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}
				if(strncmp(buffer,"exit",n-2)==0)
					break;

				//process the command
				parse_command(buffer, newsocketfd);
			
				//printf("### parse done ####\n");
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
	//int** pipefd;
	char* instruct;
	int* n_arr;

	n_pipe = num_of_pipe(command);
	n_arr = (int*) malloc((n_pipe)*sizeof(int));
	//pipefd = (int**) malloc(n_pipe * sizeof(int*));
	//for(i=0;i<n_pipe;i++){
	//	pipefd[i] = (int *) malloc(2 * sizeof(int));
	//	pipe(pipefd[i]);
	//}
	process1(command, n_arr); 	//process the pipe n number into array
	pcount=0; 					//count the order of command
	n_arr[n_pipe-1] = 1; 		//last pipe is always 1	

	instruct = strtok(command,"|\r\n");
	while(instruct != NULL){
		pair<int*,int> pipe_pair;
		pipeVec.push_back(pipe_pair);  	//push to pipe 
		pipeVec.back().first = new int[2];
		pipeVec.back().second = n_arr[pcount];
		pipe(pipeVec.back().first);

		printf("instruct %d: [%s]\n",pcount,instruct);
		pid = fork();
		if(pid==0){ //child
			char* token;


			check_update_vec();				//-- the current element in pipe

			//if(pcount != (n_pipe-1)){
				close(pipeVec.back().first[0]);
				//dup2(pipeVec.back().first[1],1);		//direct the stdout 
				dup2(pipeVec.back().first[1],2);		//direct the stdout 
				close(pipeVec.back().first[1]);
	cout<<"vector size: "<<pipeVec.size()<<endl;
	cout<<"vector back n: "<<pipeVec.back().second<<endl;
			//}

			token = strtok(instruct," \r\n");
			while(token != NULL){				//FIXME "if" should be enough
				printf("token: ##%s##\n",token);
				exec_comm(token);
				token = strtok(NULL," \r\n");
			}
			_exit(EXIT_FAILURE);
		}
		else if(pid<0){ 
			perror("parse command: fork failed");
			status = -1;
		}
		else
		{ 	//parent
			printf("parent waiting\n");
			//waitpid(pid,&status,0);
			wait(&status);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
			printf("parent done waiting\n");
		  cout<<"p1 vector size: "<<pipeVec.size()<<endl;
			remove_zero_vec();
			close(pipeVec.back().first[1]);
			pcount++; 							//instruction counter
			instruct = strtok(NULL,"|\r\n");
		  cout<<"p2 vector size: "<<pipeVec.size()<<endl;
		}
	}
	cout<<"DONE processing"<<endl;
	cout<<"vector size: "<<pipeVec.size()<<endl;
	cout<<"vector back n: "<<pipeVec.back().second<<endl;
	
	char buffer[1024];
	bzero(buffer,1024);
	while(read(pipeVec.back().first[0],buffer, sizeof(buffer)) != 0){
		pipeVec.erase(pipeVec.end());
		printf("SERVER:\n%s",buffer);
		write(sockfd,buffer,sizeof(buffer));
	}
	bzero(buffer,1024);
	printf("send back done\n");

}

void remove_zero_vec(){
	for(int i=0; i<pipeVec.size(); i++){
		cout<<"pipeVec["<<i<<"]"<<pipeVec[i].second<<endl;
		if(pipeVec[i].second == 0){
			cout<<"remove"<<i<<endl;
			pipeVec.erase(pipeVec.begin()+i);
		}
	}	
}

void check_update_vec(){
	for(int i=0; i<pipeVec.size(); i++){
		if(--pipeVec[i].second == 0){
			dup2(pipeVec[i].first[0],0);
			//pipeVec.erase(pipeVec.begin()+i);
		}
	}	
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
	//printf("exec token: ##%s##\n",token);
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
		//	execlp(args[0],args[0],args[1],NULL);
		//	printf("%s",getenv(args[1]));
		puts(getenv(args[1]));

	}else if(strcmp(token,"setenv")==0){
		//printf("here is setenv\n");
		char* args[3];
		args[0] = token;
		args[1] = strtok(NULL," \r\n");
		args[2] = strtok(NULL," \r\n");
		//printf("arg012 %s %s %s\n",args[0],args[1],args[2]);
		setenv(args[1],args[2],1);
		//execlp(args[0],args[0],args[1],args[2],NULL);

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
