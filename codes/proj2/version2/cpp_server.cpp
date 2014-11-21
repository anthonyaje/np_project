#include <iostream>
#include <cstdio>
#include <cstdlib>
//#include <errno.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <signal.h>


#define PORTNUM 8867
#define BUFFER_SIZE 10100 
#define MAX_CLIENTS 30
#define SHM_KEY 8867

using namespace std;

//GLOBAL
vector<pair<int*,int> > pipeVec;
class allUserInfo{
    public:
    int pid_arr[MAX_CLIENTS];
    int id_arr[MAX_CLIENTS];
    char name_arr[MAX_CLIENTS][20];
    char ip_arr[MAX_CLIENTS][25];
    char port_arr[MAX_CLIENTS][25];
    int fd_arr[MAX_CLIENTS];
        
};

class message{
    public:
    char mess[MAX_CLIENTS][1024];
};
//int id_list[MAX_CLIENTS]={};
allUserInfo* all_user;
message* mess_list_shm;

int shmid, shmid2;
int u_index; //user index = id;
int socketfd, newsocketfd;

//PROTOTYPE
void err_dump(char* str){	perror(str); }
int print_ip(struct sockaddr_in* cli_addr);
void process_command(char* command, int sockfd);
int num_of_pipe(char* line);
void takeout_pipe_n(char* command, int* arr); //move the pipe number from command into arr
int exec_comm(char* token, char** arg);
void decrement_vec();
int check_dup_exec_vec(char* token, char** arg);
void remove_zero_vec();
int parse_line(char** ,char*);
int validate_command(char** arg, int sockfd);
void check_redirection(char** arg);
int init_shm_user_info();
int send_welcome_note(int newsocketfd);
int request_index();

int process_chat_command(int argc,char** arg,int sockfd);
int my_shm_attach(int id);
int my_shm_deattach(int id);
int my_shm_delete(int id);
//int user_fd_to_index(int fd);
void sigusr1_handler(int n=0);
void ctrlc_handler(int n=0);
int check_name_avail(string str );
int broadcast_mess(string);
int exit_sop();

int main (int argc, char* argv[]){
	int cli_addr_size, childpid;
	struct sockaddr_in cli_addr, serv_addr;
	int n;
	char buffer[BUFFER_SIZE];	
	char* pname=argv[0];
    int portnum = PORTNUM;
	
    if(argc == 2){
        portnum = atoi(argv[1]);    
    }
	//open tcp socket
	if((socketfd=socket(AF_INET,SOCK_STREAM,0)) < 0)			
		err_dump((char*)"server: can't open stream socket");
	//bind local address
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//serv_addr.sin_addr.s_addr = inet_addr("127.1.1.1");
	serv_addr.sin_port = htons(portnum);

	if(bind(socketfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		err_dump((char*)"server: can't bind local address");

	listen(socketfd,5);
    //catch ctrl+c
    signal(SIGINT,ctrlc_handler);
    //create n init shm
    if((init_shm_user_info()) == -1){
        cout<<"shmid == -1"<<endl;
        exit(1);    
    }
    cout<<"shmid: "<<shmid<<endl;

	for(;;){
		cli_addr_size = sizeof(cli_addr);
		newsocketfd = accept(socketfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size);
		if(newsocketfd<0){
			perror("Error on accept");
			exit(1);
		}

        u_index = request_index();

        //FORKING!
		if((childpid = fork()) < 0) 
            err_dump((char*) "server: accept error");   
		else if(childpid==0){ //child process
			close(socketfd);
            sleep(1);//wait parent in

            //signal handler
            signal(SIGUSR1,sigusr1_handler);
		    send_welcome_note(newsocketfd);	
            //Tell other about me
            my_shm_attach(shmid);
            string mydetail = "*** User '("+(string)all_user->name_arr[u_index]+")' entered from "+(string)all_user->ip_arr[u_index]+"/"+(string)all_user->port_arr[u_index]+". ***\n";
            broadcast_mess(mydetail);
            my_shm_deattach(shmid);
            n = write(newsocketfd,"% ",3);
            if(n<0){
                perror("Error writing to socket");
                exit(1);
            }
            
            bzero(buffer, BUFFER_SIZE);
            setenv("PATH","bin:.",1);
            while(1){
				n = read(newsocketfd, buffer, BUFFER_SIZE);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}
                if(strncmp(buffer,"exit",4)==0){
                    //TODO
                    exit_sop();
                    cerr<<"break!!"<<endl;
                    break;
                }
                if(strncmp(buffer,"\r\n",2)==0){
                    cout<<endl;
                    write(newsocketfd,"\r\n",3);
                }
                
                printf("### parse begin ####\n");
                process_command(buffer, newsocketfd);
                printf("### parse done ####\n\n\n");
				bzero(buffer, BUFFER_SIZE);
                n = write(newsocketfd,"% ",3);
                if(n<0){
                    perror("Error writing to socket");
                    exit(1);
                }

			}
			exit(0);
		}
		close(newsocketfd); //parent process

        my_shm_attach(shmid);
        all_user->pid_arr[u_index] = childpid;
        all_user->id_arr[u_index] = u_index+1;
        strcpy(all_user->name_arr[u_index],"no name");
        strcpy(all_user->ip_arr[u_index], inet_ntoa(cli_addr.sin_addr));
        strcpy(all_user->port_arr[u_index], to_string(ntohs(cli_addr.sin_port)).c_str());
        all_user->fd_arr[u_index] = newsocketfd;
        my_shm_deattach(shmid);
        //FIXME signal child to continue

	}

return 0;
}
//--------------------------------
int init_shm_user_info(){
    key_t key = SHM_KEY;

    if((shmid = shmget(SHM_KEY,sizeof(allUserInfo),0644 | IPC_CREAT)) == -1){
        perror("shmget: ");    
        return -1;
    }
    if((all_user = (allUserInfo*) shmat(shmid,(void*)0, 0)) == (allUserInfo *)-1){
        perror("shmat: ");    
        return -1;
    }
    for(int i=0; i<MAX_CLIENTS;i++){
        all_user->pid_arr[i] = -1;    
        all_user->id_arr[i] = -1;    
        strcpy(all_user->name_arr[i],"");    
        strcpy(all_user->ip_arr[i],"");    
        strcpy(all_user->port_arr[i],"");    
        all_user->fd_arr[i] = -1;    
    }
    if(shmdt(all_user) == -1){
        perror("shmdt");
        return -1;
    }
    //init message list shm
    if((shmid2 = shmget((SHM_KEY+rand()%1000),sizeof(message),0644 | IPC_CREAT)) == -1){
        perror("shmget: ");    
        return -1;
    }
    cout<<"init_shm_user_info DONE"<<endl;
    return  0;      
}

int my_shm_attach(int id){
    if(id == shmid){
        if((all_user = (allUserInfo*) shmat(id,(void*)0, 0)) == (allUserInfo *)-1){
            perror("shmat: ");    
            return -1;
        }
    }
    else{
        if((mess_list_shm = (message*) shmat(id,(void*)0, 0)) == (message*)-1){
            perror("shmat: ");    
            return -1;
        }
    }
return 0;    
}

int my_shm_deattach(int id){
    if(id == shmid){
        if(shmdt(all_user) == -1){
            perror("shmdt");
            return -1;
        }
    }
    else{
        if(shmdt(mess_list_shm) == -1){
            perror("shmdt");
            return -1;
        }
        
    }
return 0;    
}

int my_shm_delete(int id){
    struct shmid_ds buf;
    if(shmctl(id,IPC_RMID,&buf) == -1){
        perror("shmctl");
        return -1;
    }

return 0;
}

int broadcast_mess(string mess){
    int n;
    for(int i=0; i<MAX_CLIENTS; i++){
        if(all_user->pid_arr[i]==-1)
            continue;
        my_shm_attach(shmid2);
        strcpy(mess_list_shm->mess[i],mess.c_str());
        my_shm_deattach(shmid2);
        cout<<"i "<<i<<endl;
        cout<<"br pid: "<<all_user->pid_arr[i]<<endl;
        kill(all_user->pid_arr[i],SIGUSR1);
    }

return 0;    
}

void sigusr1_handler(int n){
    my_shm_attach(shmid2);    
    string str = mess_list_shm->mess[u_index];
    my_shm_deattach(shmid2);    
    if(write(newsocketfd,str.c_str(),str.length())<0){
        perror("write err"); 
        exit(1);   
    }
    cout<<"index: "<<u_index<<"pid: "<<getpid()<<"str: "<<str;
}

void ctrlc_handler(int n){
    int ext=1;
    cout<<"CTRLC CATCHED";
    my_shm_attach(shmid);  
    for(int i=0; i<MAX_CLIENTS; i++){
        if(all_user->id_arr[i]>0){
            cout<<"there is a client to be served"<<endl;    
            ext = 0;
        }
    }  
    my_shm_deattach(shmid);    
    if(ext){
        my_shm_delete(shmid);    
        my_shm_delete(shmid2);   
        close(socketfd); 
        exit(1);
    }
}

int send_welcome_note(int newsocketfd){
    char welcomenote [] = "****************************************\n** Welcome to the information server. **\n****************************************\n";

    printf("client connection accepted!\n");
    int n = write(newsocketfd,welcomenote,sizeof(welcomenote));
    if(n<0){
        perror("Error writing from socket");
        exit(1);
    }  
    return 0; 
}


int request_index(){
    my_shm_attach(shmid);
    int ret = -1;
    for(int i=0;i<MAX_CLIENTS; i++){
        if(all_user->id_arr[i] < 0){
            all_user->id_arr[i] = 0; // -1 init, 0 taken, pid 
            ret = i;
            break;
        }    
    }

    my_shm_attach(shmid);
    return ret;    
}


int check_name_avail(string str ){
    for(int i=0; i<MAX_CLIENTS; i++){
        if(all_user->pid_arr[i]==-1)
            continue;
        if(strcmp(str.c_str(),all_user->name_arr[i])==0){
            return 0;    
        }
    }
return -1;    
}

int exit_sop(){
    my_shm_attach(shmid);
    string mydetail ="*** User '("+(string)all_user->name_arr[u_index]+")' left. ***\n"; 
    cout<<"my detail: "<<mydetail<<endl;
    broadcast_mess(mydetail);

    int i= u_index;
    all_user->pid_arr[i] = -1;    
    all_user->id_arr[i] = -1;    
    strcpy(all_user->name_arr[i],"");    
    strcpy(all_user->ip_arr[i],"");    
    strcpy(all_user->port_arr[i],"");    
    all_user->fd_arr[i] = -1;   
     
    my_shm_deattach(shmid);
    return 0;
}
/*
 ****************************************
 ***  process_chat_command()
 ***
 *****************************************
 */
int process_chat_command(int argc, char** arg,int fd){
    //Attach SHM
    my_shm_attach(shmid);
    //my_shm_attach(shmid2);
    //int fd_idx = user_fd_to_index(fd);
    if(strcmp(arg[0],"who")==0){
        string str = "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
        if(write(fd,(char*) str.c_str(),sizeof(char)*strlen(str.c_str())) <0)
            perror("Error writing to socket");

        for(int i=0; i<MAX_CLIENTS; i++){
            if(all_user->pid_arr[i] == -1)
                continue;
            string str = to_string(all_user->id_arr[i]) +"\t"+(string)all_user->name_arr[i]+"\t"+ all_user->ip_arr[i] +"/"+all_user->port_arr[i] +"\t";
            //if(all_user->fd_arr[i] == fd)
            if(all_user->id_arr[i]-1 == u_index)
                str += "<-me\n";
            else
                str += "\n";
            if(write(fd,(char*) str.c_str(),sizeof(char)*strlen(str.c_str())) <0)
                perror("Error writing to socket");
        }
    }
    else if(strcmp(arg[0],"yell")==0){
        string str = "*** "+(string) all_user->name_arr[u_index]+" yelled ***:";
        for(int i=1;i<argc;i++){
            str += " "+(string)arg[i];
        }
        str+="\n";
        broadcast_mess(str);
    }
    else if(strcmp(arg[0],"tell")==0){
        string msg;
        if(all_user->pid_arr[atoi(arg[1])-1] == -1){  //index == id-1
            msg = "*** Error: user #"+(string)arg[1]+" does not exist yet. ***\n";
            if(write(fd,msg.c_str(),sizeof(char)*strlen(msg.c_str())) < 0)
                perror("Error writing to socket");
        }
        else{
            msg = "*** "+(string)all_user->name_arr[atoi(arg[1])-1]+" told you ***: ";
            for(int i=2;i<argc;i++){
                msg += " "+(string)arg[i];
            }
            msg += "\n";
            my_shm_attach(shmid2);
            strcpy(mess_list_shm->mess[atoi(arg[1])-1],msg.c_str());
            my_shm_deattach(shmid2);
            kill(all_user->pid_arr[atoi(arg[1])-1],SIGUSR1);
                
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
            strcpy(all_user->name_arr[u_index],arg[1]);
            //assign name
            string msg = "*** User from "+(string)all_user->ip_arr[u_index]+"/"+(string)all_user->port_arr[u_index]+" is named '"+(string)arg[1]+"'. ***\n";
            cout<<"broadcasting"<<endl;
            broadcast_mess(msg);
        }
        cerr<<"name done"<<endl;

    }else{
        my_shm_deattach(shmid);
       // my_shm_deattach(shmid2);
        return 1;
    }

    my_shm_deattach(shmid);
    //my_shm_deattach(shmid2);
    return 0;
}
//--------------------------------

int print_ip(struct sockaddr_in* cli_addr){
	printf("%d.%d.%d.%d\n",
			(int)(cli_addr->sin_addr.s_addr&0xFF),
			(int)((cli_addr->sin_addr.s_addr&0xFF00)>>8),
			(int)((cli_addr->sin_addr.s_addr&0xFF0000)>>16),
			(int)((cli_addr->sin_addr.s_addr&0xFF000000)>>24));	
}


/*
 ****************************************
 ***  process_chat_command()
 ***
 *****************************************
 */
void process_command(char* command,int sockfd){
	int status,n_pipe,pcount;
	pid_t pid;
	char *token;
	int* n_arr;
	char** inst_arr;
	int fd;
    
    int n_inst;
    char* cmd;
    char command_cpy[1000];

	pcount=0; 							//count the order of command
    n_inst=0;
	n_pipe = num_of_pipe(command);
	n_arr = (int*) malloc((n_pipe)*sizeof(int));
	inst_arr = 	(char**) malloc((n_pipe)*sizeof(char*));
	
    strcpy(command_cpy,command);
	cerr<<"Command LENGTH"<<strlen(command)<<endl;
	puts(command);
	takeout_pipe_n(command, n_arr); 	//process the pipe n number into array
    printf("command: [%s]\n",command);
	n_inst = parse_line(inst_arr, command);		//cut the line @ | to array
	for(int i=0; i<n_inst;i++){
		cout<<"inst_arr[i]: "<<inst_arr[i];
		cout<<"n_arr[i]: "<<n_arr[i]<<endl;
	}

    cmd = strtok(command_cpy,"|\r\n\0");

	for(int i=0; i<n_inst; i++){
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
			    fd = open(rfilename,O_TRUNC|O_RDWR|O_CREAT,0777);
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
			break;			
		}

		if(strcmp(arg[0],"printenv")==0){
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

        if(validate_command(arg,sockfd) == -1){
            break;    
        }

        cerr<<"before process chat command"<<endl;
        if(process_chat_command(argc,arg,sockfd)==0){
            //write(sockfd,"% ",3);    
            break;
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
			    dup2(fd,1);					//direct the stdout to file
			    dup2(pipeVec.back().first[1],2);		//direct the stderr 
			    close(pipeVec.back().first[1]);
			    check_dup_exec_vec(token,arg);
			}else{
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
		token = new char[strlen(s) + 2];
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


int validate_command(char** arg,int sockfd){
    string valid_command[7]={"who","yell","tell","name","noop","printenv","setenv"};
    for(int it=0;it<7;it++){
        if(strcmp(arg[0],valid_command[it].c_str())==0){
            return 0;
        }
    }

    cerr<<"forking()"<<endl;
    int ppid = fork();
    if(ppid==0){
        int t[2];
        pipe(t);
        dup2(t[0],0);
        close(t[1]);
        int ret=0;
        ret = exec_comm(arg[0],arg);
        exit(1);
    }
    else if(ppid<0){
        return -1;
    }
    else{
        int status;
        wait(&status);
        if(status==256){
            string err_message = "Unknown command: [" + (string)arg[0] + "].\n";
            write(sockfd,err_message.c_str(),err_message.length());
            return -1;
        }
        if(status==1){
            return -1;
        }
    }
return 0;
}

int exec_comm(char* token, char** arg){
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

