//BUG FREE version
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
#include <fcntl.h>

#define PORTNUM 8867
#define BUFFER_SIZE 10100 

using namespace std;

//GLOBAL
vector<pair<int*,int> > pipeVec;
void err_dump(char* str){
	perror(str);
}
//PROTOTYPE
int print_ip(struct sockaddr_in* cli_addr);
void process_command(char* command, int sockfd);
int num_of_pipe(char* line);
void takeout_pipe_n(char* command, int* arr); //move the pipe number from command into arr
int exec_comm(char* token, char** arg);
void decrement_vec();
int check_dup_exec_vec(char* token, char** arg);
void remove_zero_vec();
void parse_line(char** ,char*);
int validate_command(char* token);
void check_redirection(char** arg);

int main (int argc, char* argv[]){
	int socketfd, newsocketfd, cli_addr_size, childpid;
	struct sockaddr_in cli_addr, serv_addr;
	int n;
	char buffer[BUFFER_SIZE];	
	char* pname=argv[0];
	
	//open tcp socket
	if((socketfd=socket(AF_INET,SOCK_STREAM,0)) < 0)			
		err_dump((char*)"server: can't open stream socket");
	//bind local address
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//serv_addr.sin_addr.s_addr = inet_addr("127.1.1.1");
	serv_addr.sin_port = htons(PORTNUM);

	if(bind(socketfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		err_dump((char*)"server: can't bind local address");

	listen(socketfd,5);
	for(;;){
		cli_addr_size = sizeof(cli_addr);
		newsocketfd = accept(socketfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size);
		if(newsocketfd<0){
			perror("Error on accept");
			exit(1);
		}
		if((childpid=fork()) < 0) err_dump((char*) "server: accept error");   
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
				n = write(newsocketfd,"% ",2);
				if(n<0){
					perror("Error writing to socket");
					exit(1);
				}

				n = read(newsocketfd, buffer, BUFFER_SIZE);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}

				if(n==2){
					cout<<endl;
				}else{
					if(strncmp(buffer,"exit",n-2)==0)
						break;
					printf("### parse begin ####\n");
					//process the command
					process_command(buffer, newsocketfd);

					printf("### parse done ####\n");
				}

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

void process_command(char* command,int sockfd){
	int status,n_pipe,pcount;
	pid_t pid;
	char *token;
	int* n_arr;
	char** inst_arr;
	int fd;

	pcount=0; 							//count the order of command
	n_pipe = num_of_pipe(command);
	n_arr = (int*) malloc((n_pipe)*sizeof(int));
	inst_arr = 	(char**) malloc((n_pipe)*sizeof(char*));
	
	cerr<<"Command "<<command<<endl;
	cerr<<"Command LENGTH"<<strlen(command)<<endl;
	
	puts(command);
	takeout_pipe_n(command, n_arr); 	//process the pipe n number into array
	puts(command);
	parse_line(inst_arr, command);		//cut the line @ | to array
	//for(int i=0; i<n_pipe;i++){
	//	cout<<"n_arr[i]: "<<n_arr[i]<<endl;
	//}

	for(int i=0; i<n_pipe; i++){
		char* arg[100]={};
		token = strtok(inst_arr[i]," \n");
		int argc=0;
		bool toFile=false;
		char* rfilename=NULL;
		char* temp;
		
		cout<<"NEW COMMAND"<<endl;
		arg[argc++] = token;
		while((temp = strtok(NULL," \n")) != NULL){
			puts(temp);
			if(strcmp(temp,">")==0){ 
			    cerr<<"to file redirection found"<<endl;
			    toFile = true; 
			    rfilename = strtok(NULL," \n");
			    fd = open(rfilename,O_RDWR|O_CREAT,777);
			    break;
 			} 
			else{
			    arg[argc] = temp;
			    printf("arg[%d]: %s\n",arg[argc]); 
			    argc++;
			}
		}
		printf("token: [%s] arg0: [%s] arg1[%s]\n",token,arg[0],arg[1]);

		if((token==NULL)){
			cerr<<"Token NULL"<<endl;
			return;			
		}

		if(validate_command(arg[0])==0){
			char unknowncomm [BUFFER_SIZE] = "Unknown Command: [";
			strcat(unknowncomm,arg[0]);
			strcat(unknowncomm, "]\n");
			write(sockfd,unknowncomm,sizeof(unknowncomm));
			cout<<unknowncomm<<endl;
			return;
		}
		if(strcmp(arg[0],"printenv")==0){
			printf("%s \n",getenv(arg[1]));
		}
		else if(strcmp(token,"setenv")==0){
			if(setenv(arg[1],arg[2],1) != 0){
				cerr<<"setenv un-successfully setted"<<endl;
			}
		}
		
		pipeVec.push_back(*(new pair<int*, int>));  	//push to pipe		
		pipeVec.back().first = new int[2];
		pipeVec.back().second = n_arr[pcount];
		pipe(pipeVec.back().first);
		cout<<pipeVec.back().first[0]<<endl;
		cout<<"before fork()"<<endl;

		pid = fork();
		if(pid == 0){
			close(pipeVec.back().first[0]);
			if(toFile == true){
			    cerr<<"redirection to file"<<endl;
			    printf("filename: [%s]\n",rfilename);
			    dup2(fd,1);					//direct the stdout to file
			    //dup2(fd,2);					//direct the stderr to file
			    close(pipeVec.back().first[1]);
			    //close(fd);
			    check_dup_exec_vec(token,arg);
		    	    //exec_comm(token,arg);
			}else{
			    cerr<<"redirection to pipe"<<endl;
			    dup2(pipeVec.back().first[1],1);		//direct the stdout to pipe
			    dup2(pipeVec.back().first[1],2);		//direct the stderr 
			    close(pipeVec.back().first[1]);
			    check_dup_exec_vec(token,arg);
			}

			_exit(EXIT_FAILURE);
		}else if(pid<0){ 
			perror("parse command: fork failed\n");
		}
		else{	
			close(pipeVec.back().first[1]);
		 	close(fd);
			printf("parent waiting\n");
			wait(&status);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
			printf("parent done waiting\n");

			cerr<<"START"<<endl;
			cerr<<"vector size: "<<pipeVec.size()<<endl;
			if(pipeVec.size()>0){
				cerr<<"vector first[0]: "<<pipeVec.back().first[0]<<endl;
				cerr<<"vector second n: "<<pipeVec.back().second<<endl;
			}
			
			if(pipeVec.back().second == -2){
				char buffer[BUFFER_SIZE];
				memset(buffer,0,BUFFER_SIZE);
				while(read(pipeVec.back().first[0],buffer, sizeof(buffer)) != 0){
					printf("SENDING: %s\n",buffer);
					write(sockfd,buffer,sizeof(buffer));
					memset(buffer,0,BUFFER_SIZE);
				}
				pipeVec.pop_back();
				printf("send back done\n");
			}
			if(pipeVec.size()>0){
				cerr<<"BEFORE REMOVE2"<<endl;
				cout<<"vector size: "<<pipeVec.size()<<endl;
				cerr<<"vector back first[0]: "<<pipeVec.back().first[0]<<endl;
				cerr<<"vector back second n: "<<pipeVec.back().second<<endl;
				//cerr<<"vector end first[0]: "<<(*pipeVec.end()).first[0]<<endl;
				//cerr<<"vector end second n: "<<(*pipeVec.end()).second<<endl;
			}
			

			remove_zero_vec();					//remove vec element whose counter = 0
			cout<<"remove done"<<endl;
			decrement_vec();					//-- each current element in pipe
			cout<<"devrement done"<<endl;

			pcount++; 							//instruction counter
			printf("parent process exit\n\n");
		}

		cout<<"DONE"<<endl;
		cout<<"vector size: "<<pipeVec.size()<<endl;
		if(pipeVec.size()>0){
			cout<<"vector first[0]: "<<pipeVec.back().first[0]<<endl;
			cout<<"vector second n: "<<pipeVec.back().second<<endl;
		}
	
	//pipeVec.erase(pipeVec.end());
	}

	pipeVec.clear();
}

void parse_line(char** arr, char* command){
	char *token, *s;
	int i=0;
	s = strtok(command,"|\r\n");
	while(s != NULL){
		token = new char[strlen(s) + 1];
		strcpy(token,s);
		token[strlen(s)]='\n';
		arr[i++] = token; 
		s = strtok(NULL,"|\r\n");
	}
}

void remove_zero_vec(){
	cout<<"start removing"<<endl;	
	cout<<"vector size: "<<pipeVec.size()<<endl;
	for(int i=0; i<pipeVec.size();i++){
		cout<<"pipe"<<i<<" :"<< pipeVec[i].first[0]<<", "<<pipeVec[i].second<<endl;
	}
/*	for(vector<pair<int*, int> >::iterator it=pipeVec.begin(); it != pipeVec.end(); ++it){
		cerr<<"pipesize :"<<pipeVec.size()<<endl;
		cerr<<"remove it.first[0]: "<<(*it).first[0]<<endl;
		cerr<<"remove it.second: "<<(*it).second<<endl;
		if(pipeVec.size()==0)
			break;	

		if((*it).second == 0){	
			cout<<"strart erasing"<<endl;	
			pipeVec.erase(it);
			cout<<"finish erasing"<<endl;	
		}else if((*it).second > 0){
			cerr<<"in if second > 0"<<endl;
		    	(*it).second--;   
		}else{
			cerr<<"unhenadled else"<<endl;
			cerr<<"pipesize"<<pipeVec.size()<<endl;
			//sleep(10);
		}
		//cerr<<"in for loop"<<endl;
	}
*/
	for(int i=0; i<pipeVec.size(); ){
		cerr<<"pipesize :"<<pipeVec.size()<<endl;
		if((*(pipeVec.begin()+i)).second == 0){	
			cout<<"strart erasing"<<endl;	
			pipeVec.erase(pipeVec.begin()+i);
			i=0;
			cout<<"finish erasing"<<endl;	
			for(int i=0; i<pipeVec.size();i++){
				cout<<"pipe"<<i<<" :"<< pipeVec[i].first[0]<<", "<<pipeVec[i].second<<endl;
			}
		}
		/*else if((*(pipeVec.begin()+i)).second > 0){
			cerr<<"in if second > 0"<<endl;
		    	(*(pipeVec.begin()+i)).second--;   
			i++;
		}
		*/
		else{
			cerr<<"unhenadled else"<<endl;
			cerr<<"pipesize"<<pipeVec.size()<<endl;
			i++;
			//sleep(10);
		}
		//cerr<<"in for loop"<<endl;
	}


/*	if( (pipeVec.back().second > 0)){
		pipeVec.back().second--;
	} 
	else if(pipeVec.back().second == 0){ 
		pipeVec.pop_back();
	}
*/	cout<<"AFTER vector size: "<<pipeVec.size()<<endl;
	for(int i=0; i<pipeVec.size();i++){
		cout<<"pipe"<<i<<" :"<< pipeVec[i].first[0]<<", "<<pipeVec[i].second<<endl;
	}
	cout<<endl;

    
}

void decrement_vec(){
	for(int i=0; i<pipeVec.size(); i++){
		--pipeVec[i].second;
	}	
}

int check_dup_exec_vec(char* token, char** arg){
	int t_pipe[2];
	pipe(t_pipe);
//	cerr<<"in check_dup_exec"<<endl ;
//	cerr<<"vect size"<<pipeVec.size()<<endl;	

	for(int i=0; i<pipeVec.size(); i++){
//		cerr<<"pipeVec first[0]:"<<pipeVec[i].first[0]<<endl;
//		cerr<<"pipeVec second:"<<pipeVec[i].second<<endl;
		if(pipeVec[i].second == 0){
			char buff[BUFFER_SIZE]={""};
			memset(buff,0,BUFFER_SIZE);
			read(pipeVec[i].first[0],buff,BUFFER_SIZE);
			write(t_pipe[1],buff,strlen(buff));
			
//			cerr<<"if pipeVec first[0]:"<<pipeVec[i].first[0]<<endl;
//			cerr<<"if buff: \n"<<buff;
		}
	}	
	
	dup2(t_pipe[0],0);
	close(t_pipe[1]);
//	cerr<<"returning"<<endl ;

return	exec_comm(token, arg); 
}

int num_of_pipe(char* line){  			//same as num of instruction
	int i,n;
	n=0;
	for(i=0; i<strlen(line); i++){
		if(line[i]=='|') n++;		
	}

return n+1;
}

void takeout_pipe_n(char* command, int* arr){
	char* it=command;
	int i,j,n,n_pipe;
	j=0;

	n_pipe = num_of_pipe(command);
	arr[n_pipe-1] = -2;


	n = strlen(command);
	for(i=0; i<n; i++){
		if(*(it+i) == '|'){
			if( (*(it+i+1)==' ') || (*(it+i+1)=='\r') || (*(it+i+1)=='\n') )
				arr[j++] = 1;
			else{
				arr[j++] = atoi(it+i+1);
				*(it+i+1) = ' ';
			}
		}
	}

/*	if((*(it+n-3)!='|')  && (*(it+n-4)!='|')){
		cerr<<"n-3: "<<*(it+n-3)<<"    n-4: "<<*(it+n-4)<<endl;
		arr[j] = -2;			
	}
*/

	for(int i=0;i<n_pipe;i++){
	    cout<<"arr "<<i<<": "<<arr[i]<<endl;
	}
}

int validate_command(char* token){
	char* valid_command[7]={"ls","cat","printenv","setenv","number","removetag","removetag0"};
	for(int it=0;it<7;it++){			
		if(strcmp(token,valid_command[it])==0){
			return 1;
		}
	}
	return 0;
}
/*
void stdout_redirection(char** arg, int n){
	char* it = *arg;
	while(it!=NULL){
	    dup2(pipeVec.back().first[1],1);		//direct the stdout 
	    if(*(it+1) != NULL)
	}
	
}
*/
int exec_comm(char* token, char** arg){
//	cerr<<"in exec command"<<endl;	
	if(validate_command(arg[0]) == 0){
		return 0;
	}	
	if(strcmp(token,"printenv")==0){
	   printf("%s \n",getenv(arg[1]));

	}else if(strcmp(token,"setenv")==0){
		int ret_val;
		if(unsetenv(arg[1]) == 0){
//			cerr<<"setenv successfully setted"<<endl;
		}
		if(setenv(arg[1],arg[2],1)==0){
//			cerr<<"setenv successfully setted"<<endl;
		}
	}
    	else{	
		execvp(arg[0],arg);
	}

}	

