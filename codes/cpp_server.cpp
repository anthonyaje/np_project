#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/socket.h>
//#include <errno.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <vector>
#include <fcntl.h>

#define PORTNUM 8867
#define BUFFER_SIZE 10100 
#define MAX_CLIENT 5 

using namespace std;

//GLOBAL
class user{
    public:
    int id;
    string name;
    string ip;
    string port;
    int fd;
    int p_ready;
    int pipefd[2];
    //vector<string> env_name;
    //vector<string> env_addr;
    };
vector<pair<int*,int> > pipeVec;
vector<user> user_list;

//PROTOTYPE
void err_dump(char* str){
	perror(str);
}
int print_ip(struct sockaddr_in* cli_addr);
void process_command(char* command, int sockfd);
int num_of_pipe(char* line);
void takeout_pipe_n(char* command, int* arr); //move the pipe number from command into arr
int exec_comm(char* token, char** arg);
void decrement_vec();
int check_dup_exec_vec(char* token, char** arg);
void remove_zero_vec();
int parse_line(char** ,char*);
int validate_command(char* token);
void check_redirection(char** arg);

int remove_user(vector<user>& user_list, int fd, int* id_list);
int broadcast_mess(string& mess, vector<user>& list, int myfd,int notme);
int process_chat_command(int argc, char** arg, int fd);
int request_id(int* arr);
int check_name_avail(string name);
int user_fd_to_index(int fd);
int user_id_to_fd(int id); 

int main (int argc, char* argv[]){
	int listenfd, connfd, cli_addr_size, childpid;
	struct sockaddr_in cli_addr, serv_addr;
	int n;
	char buffer[BUFFER_SIZE];	
	char* pname=argv[0];
	int port_num = PORTNUM;	
	fd_set read_set, ready_set;	
	int nfds;
    int user_count=0;
    int id_list[31]={};

	if(argc == 2){
	    port_num = atoi(argv[1]);	
	}

	//open tcp socket
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)			
		err_dump((char*)"server: can't open stream socket");
	//bind local address
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port_num);

	if(bind(listenfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		err_dump((char*)"server: can't bind local address");

	listen(listenfd,MAX_CLIENT);
    cout<<"listenfd "<<listenfd<<endl;
	FD_ZERO(&read_set);
	FD_ZERO(&ready_set);
	FD_SET(listenfd, &read_set);	
	nfds = listenfd+1;	
	//nfds = 256;	

	for(;;){
        cout<<"user_list.size(): "<<user_list.size()<<endl;

		ready_set = read_set;
        //SELECT HERE
        cout<<"running select"<<endl;
		int nready = select(nfds,&ready_set,(fd_set*)0,(fd_set*)0,(struct timeval*)0);
        if(nready<0){ perror("Error in select\n"); }
		if(FD_ISSET(listenfd,&ready_set)){
			cli_addr_size = sizeof(cli_addr);
			if( (connfd = accept(listenfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size)) <0){
				perror("Error on accept");
				exit(1);
			}
			char welcomenote [200] = "****************************************\n** Welcome to the information server. **\n****************************************\n";

			printf("client connection accepted!\n");
			n = write(connfd,welcomenote,sizeof(welcomenote));
			if(n<0){ perror("Error writing to socket"); exit(1); }
            
            user u;        
            u.id = request_id(id_list);
            u.name = "no name";
            u.ip = (string)(inet_ntoa(cli_addr.sin_addr));
            u.port = to_string(ntohs(cli_addr.sin_port));
            u.fd = connfd;
            u.p_ready = -1;
            //pipe(u.pipefd);

            user_list.push_back(u);
            
            string mydetail = "*** User '("+u.name+")' entered from "+u.ip+"/"+u.port+". ***\n";
            //Tell other about me
            broadcast_mess(mydetail,user_list,connfd,0);
        
            cout<<"print % "<<endl;
            n = write(connfd,"% ",sizeof(char)*strlen("% "));
            if(n<0){ perror("Error writing to socket"); exit(1);}


            cout<<"connfd"<<connfd<<endl;
            user_count++;
			FD_SET(connfd,&read_set);
            //adjust the upperbound for the ready_set
            if(connfd >= nfds)
                nfds = connfd+1;
		}
		
        cout<<"nfds "<<nfds<<endl;
        int nfds_cpy = nfds;
		for(int it_fd=0; it_fd<nfds_cpy; it_fd++){
            if(it_fd!=listenfd && FD_ISSET(it_fd,&ready_set)){
                bzero(buffer, BUFFER_SIZE);
                setenv("PATH","bin:.",1);
                n = read(it_fd, buffer, BUFFER_SIZE);
                if(n<0){ perror("Error reading from socket"); exit(1);}
                if(strncmp(buffer,"exit",4)==0){
                    //remove user from list
                    remove_user(user_list,it_fd, id_list);
                    cout<<"after remove user"<<endl;
                    close(it_fd);
                    FD_CLR(it_fd,&read_set);
                    user_count--;
                    //nfds--;
                    cerr<<"exit!!"<<endl;
                    continue; 
                }
                int savstdout = dup(1);

                printf("### parse begin ####\n");
                process_command(buffer, it_fd);
                printf("### parse done ####\n\n\n");

                fflush(stdout);
                dup2(savstdout,1);
                
                cout<<"print % "<<endl;
                char beginchar[5]="% ";
                n = write(it_fd,beginchar,sizeof(beginchar));
                if(n<0){ perror("Error writing to socket"); exit(1);}

                bzero(buffer, BUFFER_SIZE);
            }
		}
	}

return 0;
}

int process_chat_command(int argc, char** arg,int fd){
    if(strcmp(arg[0],"who")==0){
        string str = "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
        if(write(fd,(char*) str.c_str(),sizeof(char)*strlen(str.c_str())) <0)
            perror("Error writing to socket"); 
        for(int i=0; i<user_list.size(); i++){
           str = to_string(user_list[i].id) +"\t"+ user_list[i].name+"\t"+ user_list[i].ip +"/"+ user_list[i].port+"\t";
           if(user_list[i].fd == fd)
               str += "<-me\n";
           else
               str += "\n";
           if(write(fd,(char*) str.c_str(),sizeof(char)*strlen(str.c_str())) <0)
               perror("Error writing to socket"); 
        }
    }
    else if(strcmp(arg[0],"yell")==0){
        string msg = "*** "+user_list[user_fd_to_index(fd)].name+" yelled ***:";
        for(int i=1;i<argc;i++){
            msg += " "+(string)arg[i];    
        }
        msg+="\n";
        broadcast_mess(msg,user_list,fd,0);
    
    }
    else if(strcmp(arg[0],"tell")==0){
        int to_fd = user_id_to_fd(atoi(arg[1]));
        string my_name = user_list[user_fd_to_index(fd)].name;
        string msg;
        if(to_fd == -1){
            msg = "*** Error: user #"+(string)arg[1]+" does not exist yet. ***\n";
            if(write(fd,msg.c_str(),sizeof(char)*strlen(msg.c_str())) < 0)
                perror("Error writing to socket"); 
        }
        else{
            msg = "*** "+my_name+" told you ***: ";
            for(int i=2;i<argc;i++){
                msg += " "+(string)arg[i];    
            }
            msg += "\n";  
            if(write(to_fd,msg.c_str(),sizeof(char)*strlen(msg.c_str())) < 0)
                perror("Error writing to socket"); 
        }
        

    }
    else if(strcmp(arg[0],"name")==0){
        //check validity
        if(check_name_avail((string) arg[1])==0){
            string msg = "*** User '("+(string)arg[1]+")' already exists. ***\n";
            if(write(fd,msg.c_str(),sizeof(char)*strlen(msg.c_str())) < 0)
               perror("Error writing to socket"); 
        }
        else{
            //assign name
            int i = user_fd_to_index(fd); 
            user_list[i].name = arg[1];
            string msg = "*** User from "+user_list[i].ip+"/"+user_list[i].port+" is named '"+user_list[i].name+"'. ***\n";
            broadcast_mess(msg,user_list,fd,0);        
        }

    }else{
        return 1;    
    }

return 0;    
}
int user_id_to_fd(int id){
    for(int i=0;i<user_list.size();i++){
        if(user_list[i].id == id){
            return user_list[i].fd;
        }    
    }
cerr<<"user_id_to_fd not found"<<endl;
return -1;
        
}

int user_fd_to_index(int fd){
    for(int i=0;i<user_list.size();i++){
        if(user_list[i].fd == fd){
            return i;
        }    
    }
cerr<<"user_fd_to_index"<<endl;
return -1;
}
int check_name_avail(string name){
    for(int i=0; i<user_list.size(); i++){
        if(user_list[i].name.compare(name)==0)
              return 0; 
    }

return 1;    
}

int request_id(int* arr){
    for(int i=1; i<=30; i++)
        if(arr[i]==0){
            arr[i]=-1;
            return i;    
        }
   return 0;
}

int broadcast_mess(string& mess, vector<user>& list,int myfd, int notme){
    int n;
    if(notme){
        for(int i=0; i<list.size(); i++){
            if(list[i].fd != myfd){
                n = write(list[i].fd,(char*) mess.c_str(),sizeof(char)*strlen(mess.c_str()) );
                if(n<0){ perror("Error writing to socket"); }
            }
        }
    }
    else{
        for(int i=0; i<list.size(); i++){
            cout<<"to: "<<list[i].id<<endl;
                n = write(list[i].fd,(char*) mess.c_str(),sizeof(char)*strlen(mess.c_str()) );
                if(n<0){ perror("Error writing to socket"); }
        }
    }

return 0;
}

int remove_user(vector<user>& user_list, int fd, int* id_list){
    for(int i=user_list.size()-1; i>=0; i--)
        if(user_list[i].fd == fd){
            id_list[user_list[i].id] = 0;
            string mess = "*** User '("+user_list[i].name+")' left. ***\n";
            broadcast_mess(mess,user_list,fd,0);
            user_list.erase(user_list.begin()+i);    
        }

    return 0;
}

void process_command(char* command,int sockfd){
	int status,n_pipe,pcount, n_inst;
	pid_t pid;
	char *token;
	int* n_arr;
	char** inst_arr;
	int fd,cli_fd0,cli_fd1;

	pcount=0; 							//count the order of command
    n_inst=0;
	n_pipe = num_of_pipe(command);
	n_arr = (int*) malloc((n_pipe)*sizeof(int));
	inst_arr = 	(char**) malloc((n_pipe)*sizeof(char*));
	
	cerr<<"Command LENGTH"<<strlen(command)<<endl;
	puts(command);
	takeout_pipe_n(command, n_arr); 	//process the pipe n number into array
	//puts(command);
    printf("command: [%s]\n",command);
	n_inst = parse_line(inst_arr, command);		//cut the line @ | to array
	//for(int i=0; i<n_pipe;i++){
	for(int i=0;i<n_inst;i++){
		cout<<"inst_arr[i]: "<<inst_arr[i]<<endl;
		cout<<"n_arr[i]: "<<n_arr[i]<<endl;
	}

	//for(int i=0; i<n_pipe; i++){
	for(int i=0; i<n_inst; i++){
		char* arg[100]={};
		token = strtok(inst_arr[i]," \n");
		int argc=0;
		bool toFile=false;
		bool toClientPipe=false;
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
			    fd = open(rfilename,O_TRUNC|O_RDWR|O_CREAT,0777);
			    break;
 			} 
			else{
                if(*temp=='>'){
			        cerr<<"to other client redirection found"<<endl;
                    int dest_id = atoi((temp+1));
                    int u_index = user_fd_to_index(sockfd);  
                    int source_id = user_list[u_index].id;
                    int dest_fd = user_id_to_fd(dest_id);
                    if(dest_fd == -1){
                        string str = "*** Error: user #"+to_string(dest_id)+" does not exist yet. ***\n";
                        write(sockfd, str.c_str(),sizeof(char)*strlen(str.c_str()));

                        break;
                    }
                    else{
                        if(user_list[u_index].p_ready == -1){ //uninit
                            toClientPipe=true;
                            pipe(user_list[u_index].pipefd);
                            cli_fd0 = user_list[u_index].pipefd[0];
                            cli_fd1 = user_list[u_index].pipefd[1];
                            user_list[u_index].p_ready = 1; 
                        }
                        else if(user_list[u_index].p_ready == 0){ //ready
                            toClientPipe=true;
                            cli_fd0 = user_list[u_index].pipefd[0];
                            cli_fd1 = user_list[u_index].pipefd[1];
                            user_list[u_index].p_ready = 1; 
                        }
                        else{   // 1 if it has been used
                            toClientPipe=false;   
                            string str = " *** Error: the pipe #"+to_string(source_id)+"->#"+to_string(dest_id)+" already exists. ***\n";
                            write(sockfd, str.c_str(),sizeof(char)*strlen(str.c_str()));

                            break;
                        }
                    }
                    break;
                }
                else if(*temp=='<'){
                    //TODO get from pipe

                }
                else{
                    arg[argc] = temp;
                    printf("arg[%d]: %s\n",argc,arg[argc]); 
                    argc++;
                }
			}
		}
		printf("token: [%s] arg0: [%s] arg1[%s]\n",token,arg[0],arg[1]);

		if((token==NULL)){
			cerr<<"Token NULL"<<endl;
			break;			
		}

		if(validate_command(arg[0])==0){
			char unknowncomm [BUFFER_SIZE] = "Unknown command: [";
			strcat(unknowncomm,arg[0]);
			strcat(unknowncomm, "].\n");
			write(sockfd,"eatme\r\n",7);
			write(sockfd,unknowncomm,sizeof(unknowncomm));
			cout<<unknowncomm<<endl;
			break;
		}

		if(strcmp(arg[0],"printenv")==0){
            cout<<"inside printenv"<<endl;
			char str[1000]="";
			strcat(str,arg[1]);
			strcat(str,"=");
			strcat(str,getenv(arg[1]));
			strcat(str,"\n");
			write(sockfd,"eatme\r\n",7);
			write(sockfd,str,sizeof(str));
			break;
		}
		else if(strcmp(token,"setenv")==0){
			if(setenv(arg[1],arg[2],1) != 0){
				cerr<<"setenv un-successfully setted"<<endl;
			}
			break;
		}
        cerr<<"before process chat command"<<endl;
        if(process_chat_command(argc,arg,sockfd)==0)
            break;
		
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
			    dup2(fd,1);					//direct the stdout to file
			    dup2(pipeVec.back().first[1],2);		//direct the stderr 
			    close(pipeVec.back().first[1]);
			    check_dup_exec_vec(token,arg);
			}
            else if(toClientPipe){
                cerr<<"dup to client"<<endl;
                dup2(cli_fd1,1);   
                dup2(cli_fd1,2);
                close(cli_fd1);   
			    check_dup_exec_vec(token,arg);
            }
            else{
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
			wait(&status);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
			if(pipeVec.back().second == -2){
				char buffer[BUFFER_SIZE];
				memset(buffer,0,BUFFER_SIZE);
				while(read(pipeVec.back().first[0],buffer, sizeof(buffer)) != 0){
					write(sockfd,"eatme\r\n",7);
					write(sockfd,buffer,BUFFER_SIZE);
				}
				close(pipeVec.back().first[0]);
				pipeVec.pop_back();
			}
				
			remove_zero_vec();					//remove vec element whose counter = 0
			decrement_vec();					//-- each current element in pipe

			pcount++; 							//instruction counter
		}

	}
}

int parse_line(char** arr, char* command){
	char *token, *s;
	int i=0;
	s = strtok(command,"|\r\n");
	while(s != NULL){
		token = new char[strlen(s)+2];
		strcpy(token,s);
		token[strlen(s)+1]='\n';
		arr[i++] = token; 
		s = strtok(NULL,"|\r\n");
	}
    return i;
}

void remove_zero_vec(){
	for(int i=pipeVec.size()-1; i>=0; i--){
		if((*(pipeVec.begin()+i)).second == 0){	
			close((*(pipeVec.begin()+i)).first[0]);
			pipeVec.erase(pipeVec.begin()+i);
		}
	}

}

void decrement_vec(){
	for(int i=0; i<pipeVec.size(); i++){
		--pipeVec[i].second;
	}	
}

int check_dup_exec_vec(char* token, char** arg){
	int t_pipe[2];
	pipe(t_pipe);

	for(int i=0; i<pipeVec.size(); i++){
		if(pipeVec[i].second == 0){
			char buff[BUFFER_SIZE]={""};
			memset(buff,0,BUFFER_SIZE);
			read(pipeVec[i].first[0],buff,BUFFER_SIZE);
			write(t_pipe[1],buff,strlen(buff));
		}
	}	
	
	dup2(t_pipe[0],0);
	close(t_pipe[1]);

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
				char num [10]={};
				int k=0;
				while(1){
					if((*(it+i+1) == '\0') || (*(it+i+1) == ' ') || (*(it+i+1) == '\r') || (*(it+i+1) == '\n')){
						break;
					}
					if(((int)(*(it+i+1))) == 0){
						break;
					}
					num[k++]=*(it+i+1);
					*(it+i+1) = ' ';
					i++;
				}
				arr[j++] = atoi(num);
			}
		}
	}
}

int validate_command(char* token){
	string valid_command[12]={"who","yell","tell","name","noop","ls","cat","printenv","setenv","number","removetag","removetag0"};
	for(int it=0;it<12;it++){			
		if(strcmp(token,valid_command[it].c_str())==0){
			return 1;
		}
	}
	return 0;
}

int exec_comm(char* token, char** arg){
	if(validate_command(arg[0]) == 0){
		return 0;
	}	
	if(strcmp(token,"printenv")==0){
	   printf("%s \n",getenv(arg[1]));

	}else if(strcmp(token,"setenv")==0){
		int ret_val;
		if(setenv(arg[1],arg[2],1)==0){
			cerr<<"setenv successfully setted"<<endl;
		}
	}
    	else{	
		execvp(arg[0],arg);
	}

}	

