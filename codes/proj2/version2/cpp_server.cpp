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
#include <sys/stat.h>

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
    int fifo_checklist[MAX_CLIENTS+1][MAX_CLIENTS+1];
    char dead_list[MAX_CLIENTS][30];
    int dead_i;
        
};

class message{
    public:
    char mess[MAX_CLIENTS][1024];
};
//int id_list[MAX_CLIENTS]={};
allUserInfo* all_user;
message* mess_list_shm;

int shmid, shmid2;
int u_index;                                //user index = id;
int socketfd, newsocketfd;
int fifofd[MAX_CLIENTS+1][MAX_CLIENTS+1];
string fifo_path = "/tmp/aje_fifo_";
bool dontDup=false, toClientPipe=false, dupStdin=false;
int dest_id;
char* cmd;
int w_cli_fd, r_cli_fd, file_fd;
string to_cli_msg_r;
string to_cli_msg_w;

//PROTOTYPE
void err_dump(char* str){	perror(str); }
int print_ip(struct sockaddr_in* cli_addr);
void process_command(char* command, int sockfd);
int num_of_pipe(char* line);
void takeout_pipe_n(char* command, int* arr); //move the pipe number from command into arr
int exec_comm(char* token, char** arg);
void decrement_vec();
int check_dup_exec_vec(char* token, char** arg, int ext_pipe_fd=-1);
void remove_zero_vec();
int parse_line(char** ,char*);
int validate_command(char** arg, int sockfd);
void check_redirection(char** arg);
int init_shm_user_info();
int send_welcome_note(int newsocketfd);
int request_index();

int process_chat_command(char* inst,int argc,char** arg,int sockfd);
int my_shm_attach(int id);
int my_shm_deattach(int id);
int my_shm_delete(int id);
//int user_fd_to_index(int fd);
void sigusr1_handler(int n=0);
void ctrlc_handler(int n=0);
int check_name_avail(string str );
int broadcast_mess(string mess,int notme=0);
int exit_sop();
void fifo_init();
int fifo_write(string mes, int from, int to);
int fifo_read(char* buff, int from, int to);
void fifo_close(int from, int to);
int check_to_clientpipe_redir(char* temp);

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
    //FIFO 
    fifo_init();    // fifofd[30][30] fifo and RDONLY NOONBLOCKING

	for(;;){
		cli_addr_size = sizeof(cli_addr);
		newsocketfd = accept(socketfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size);
		if(newsocketfd<0){
			perror("Error on accept");
			exit(1);
		}

        u_index = request_index();
        my_shm_attach(shmid);
        all_user->id_arr[u_index] = u_index+1;
        strcpy(all_user->name_arr[u_index],"(no name)");
        //strcpy(all_user->ip_arr[u_index], inet_ntoa(cli_addr.sin_addr));
        strcpy(all_user->ip_arr[u_index], "CGILAB");
        //strcpy(all_user->port_arr[u_index], to_string(ntohs(cli_addr.sin_port)).c_str());
        strcpy(all_user->port_arr[u_index], "511");
        all_user->fd_arr[u_index] = newsocketfd;
        my_shm_deattach(shmid);

        //FORKING!
		if((childpid = fork()) < 0) 
            err_dump((char*) "server: accept error");   
		else if(childpid==0){ //child process
			close(socketfd);
            //sleep(1);//wait parent in
            //signal handler
            signal(SIGUSR1,sigusr1_handler);
		    send_welcome_note(newsocketfd);	
            //Tell other about me
            my_shm_attach(shmid);
            string mydetail = "*** User '"+(string)all_user->name_arr[u_index]+"' entered from "+(string)all_user->ip_arr[u_index]+"/"+(string)all_user->port_arr[u_index]+". ***\n";
            broadcast_mess(mydetail,1);
            my_shm_deattach(shmid);
            /*n = write(newsocketfd,"% ",3);
            if(n<0){
                perror("Error writing to socket");
                exit(1);
            }*/
            
            bzero(buffer, BUFFER_SIZE);
            setenv("PATH","bin",1);
            while(1){
				n = read(newsocketfd, buffer, BUFFER_SIZE);
				if(n<0){
					perror("Error reading from socket");
					exit(1);
				}
                if(strncmp(buffer,"exit",4)==0){
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

			}
			exit(0);
		}
        my_shm_attach(shmid);
        all_user->pid_arr[u_index] = childpid;
        my_shm_deattach(shmid);
		close(newsocketfd); //parent process
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
        all_user->dead_i = 0;
        for(int j=1; j<MAX_CLIENTS+1; j++){
            all_user->fifo_checklist[i+1][j] = -1;    
        } 
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

void fifo_init(){
    for(int i=1; i<MAX_CLIENTS+1;i++){
        for(int j=1; j<MAX_CLIENTS+1; j++){
            string fifo_name = fifo_path+to_string(i)+"_"+to_string(j);
            if(mknod(fifo_name.c_str(),S_IFIFO | 0666,0) < 0){
                perror("mknod fifo");
            }
            
            if((fifofd[i][j] = open(fifo_name.c_str(), O_NONBLOCK|O_RDONLY)) < 0){
               perror("open fifo");     
            }     
        }
    }
}

int fifo_write(string mes,int from , int to){
    int fd;
    string fifoname = fifo_path + to_string(from) +"_"+to_string(to);
    if((fd = open(fifoname.c_str(), O_NONBLOCK | O_WRONLY) < 0)){
        perror("open");
        return -1;
    }    
    if(write(fd,mes.c_str(),mes.length())){
        perror("write");    
        return -1;
    }
    close(fd);

return 0;
}

int fifo_read(char* buff, int from, int to){
    //char buffer[BUFFER_SIZE];
    if(read(fifofd[from][to],buff,BUFFER_SIZE) < 0){
        perror("write");
        return -1;
    }
    
return 0;    
}

void fifo_close(int from, int to){
    string fifoname = fifo_path+to_string(from)+"_"+to_string(to);
    close(fifofd[from][to]);
    unlink(fifoname.c_str());
}

int broadcast_mess(string mess, int notme){
    //my_shm_attach(shmid);
    cout<<"broadcasting"<<endl;
    int n;
    for(int i=0; i<MAX_CLIENTS; i++){
        if(all_user->pid_arr[i]==-1)
            continue;
        if(notme)
            if(all_user->pid_arr[i]==getpid())
                continue;
        my_shm_attach(shmid2);
        strcpy(mess_list_shm->mess[i],mess.c_str());
        my_shm_deattach(shmid2);
        cout<<"i "<<i<<endl;
        cout<<"br pid: "<<all_user->pid_arr[i]<<endl;
        kill(all_user->pid_arr[i],SIGUSR1);
    }
   // my_shm_deattach(shmid);
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
        my_shm_attach(shmid);  
        cout<<"closing fifo loop, userid: "<<u_index+1<<endl;
        for(int i=1;i<MAX_CLIENTS+1; i++){
            for(int j=1;j<MAX_CLIENTS+1; j++){
                all_user->fifo_checklist[i][j]=-1;
                fifo_close(i,j);
                if(i!=j)
                    fifo_close(j,i);
            }
        }
        my_shm_deattach(shmid);  
         
        my_shm_delete(shmid);    
        my_shm_delete(shmid2);   
        close(socketfd); 
        exit(1);
    }
}

int exit_sop(){
    my_shm_attach(shmid);
    string mydetail ="*** User '"+(string)all_user->name_arr[u_index]+"' left. ***\n"; 
    cout<<"my detail: "<<mydetail<<endl;
    broadcast_mess(mydetail);

    int i= u_index;
    all_user->pid_arr[i] = -1;    
    all_user->id_arr[i] = -1;    
    strcpy(all_user->name_arr[i],"");    
    strcpy(all_user->ip_arr[i],"");    
    strcpy(all_user->port_arr[i],"");    
    all_user->fd_arr[i] = -1;  
    
    char bufff[1024];
    cout<<"resetting fifo , userid: "<<u_index+1<<endl;
    for(int j=1;j<MAX_CLIENTS+1; j++){
       all_user->fifo_checklist[u_index+1][j]=-1;
       all_user->fifo_checklist[j][u_index+1]=-1;
       fifo_read(bufff,u_index+1,j);
       fifo_read(bufff,j,u_index+1);
    } 
     
    my_shm_deattach(shmid);
    return 0;
}


int send_welcome_note(int newsocketfd){
    char welcomenote [] = "****************************************\n** Welcome to the information server. **\n****************************************\n";

    printf("client connection accepted!\n");
    my_shm_attach(shmid);
    string mydetail = "*** User '"+(string)all_user->name_arr[u_index]+"' entered from "+(string)all_user->ip_arr[u_index]+"/"+(string)all_user->port_arr[u_index]+". ***\n";
    my_shm_deattach(shmid);
    mydetail = (string) welcomenote + mydetail + "% ";
    int n = write(newsocketfd,mydetail.c_str(),mydetail.length());
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

/*
 ****************************************
 ***  process_chat_command()
 ***
 *****************************************
 */
int process_chat_command(char* inst, int argc, char** arg,int fd){
    //Attach SHM
    my_shm_attach(shmid);
    char* comm;
    char* rest;
    comm = strtok(inst," \r\n");
    rest = strtok(NULL,"\r\n");
    cerr<<"|||||||||||||||||||||||||||||||||||||||||||||||||||"<<endl;
    cerr<<"comm: "<<comm<<endl;
    cerr<<"rest: "<<rest<<endl;
   // cerr<<"arg[0]"<<arg[0]<<endl;
    //my_shm_attach(shmid2);
    //int fd_idx = user_fd_to_index(fd);
    cerr<<"if SUICIDING"<<endl;
    if(strcmp(comm,"suicide")==0){
        cerr<<"cerr SUICIDING"<<endl;
        cout<<"cout SUICIDING"<<endl;
        if(strcmp(all_user->name_arr[u_index],"(no name)")==0){
          cerr<<"NO NAME cant be deleted"<<endl;  
        };
        strcpy(all_user->dead_list[all_user->dead_i],all_user->name_arr[u_index]);
        cerr<<"BLACKLIST"<<all_user->dead_list[all_user->dead_i]<<endl;
        cout<<"cout BLACKLIST"<<all_user->dead_list[all_user->dead_i]<<endl;
        string s = "*** the user ";
        s = s + all_user->name_arr[u_index] + " is dead ***\n";
        broadcast_mess(s);
        all_user->dead_i++;
        cout<<"dead_i"<<all_user->dead_i<<endl;
        sleep(1);
        exit_sop();
        close(newsocketfd);
        return 0;
    }

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
        /*for(int i=1;i<argc;i++){
            str += " "+(string)arg[i];
        }
        */
        str+=rest;
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
            cerr<<"before strtok"<<endl;
            comm = strtok(rest," \r\n");
            cerr<<"after strtok:    "<<comm<<endl;
            rest = strtok(NULL,"\r\n");
            msg = "*** "+(string)all_user->name_arr[u_index]+" told you ***: ";
            /*for(int i=2;i<argc;i++){
                msg += " "+(string)arg[i];
            }
            */
            msg+=rest;
            msg += "\n";
            my_shm_attach(shmid2);
            strcpy(mess_list_shm->mess[atoi(arg[1])-1],msg.c_str());
            my_shm_deattach(shmid2);
            kill(all_user->pid_arr[atoi(arg[1])-1],SIGUSR1);
                
        }

    }
    else if(strcmp(arg[0],"name")==0){
        for(int i=0; i<all_user->dead_i; i++){
            cout<<"checking deadlist"<<endl;
            cout<<"dead_i"<<all_user->dead_i<<endl;
            cout<<"dead_list[i]"<<all_user->dead_list[i]<<endl;    
            if(strcmp(arg[1],all_user->dead_list[i])==0){
                string s = "*** the user ";
                s = s + all_user->name_arr[u_index] + " is dead ***\n";
                write(fd,s.c_str(),sizeof(char)*strlen(s.c_str()));
                return 0;
            }    
        }
        //check validity
        if(check_name_avail((string) arg[1])==0){
            string msg = "*** User '"+(string)arg[1]+"' already exists. ***\n";
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
	int status,n_pipe,pcount,n_inst;
	pid_t pid;
	char *token;
	int* n_arr;
	char** inst_arr;
    char command_cpy[1000];
    bool toFile =  false;
    toClientPipe=false;
    dontDup=false;
    dupStdin=false;


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
    cmd = strtok(command_cpy,"\r\n\0");
	for(int i=0; i<n_inst; i++){
		char* arg[100]={};
		int argc=0;
		char* rfilename=NULL;
		char* temp;
		int n;

		cout<<"NEW COMMAND"<<endl;
		token = strtok(inst_arr[i]," \n");
		arg[argc++] = token;
		while((temp = strtok(NULL," \n")) != NULL){
			puts(temp);
			if(strcmp(temp,">")==0){ 
			    cerr<<"to file redirection found"<<endl;
			    toFile = true; 
			    rfilename = strtok(NULL," \n");
			    break;
 			}
			else{
                if((n=check_to_clientpipe_redir(temp))<0){
                    //break;    
                    return;
                }
                else if(n==1){
                    //break;    
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
		if(strcmp(arg[0],"printenv")==0){
            string str = (string)arg[1] + "="+(string) getenv(arg[1])+"\n";
			write(sockfd,str.c_str(),str.length());
			break;
		}
		else if(strcmp(token,"setenv")==0){
			if(setenv(arg[1],arg[2],1) != 0){
				cerr<<"setenv un-successfully setted"<<endl;
			}
			break;
		}
        cerr<<"!before process chat command!"<<endl;
        cout<<"!before process chat command!"<<endl;
        if(process_chat_command(command_cpy,argc,arg,sockfd)==0){
            //write(sockfd,"% ",3);    
            break;
        }
        if(validate_command(arg,sockfd) == -1){
            break;    
        }
		
		pipeVec.push_back(*(new pair<int*, int>));  	//push to pipe		
		pipeVec.back().first = new int[2];
		pipeVec.back().second = n_arr[pcount];
		pipe(pipeVec.back().first);

        int* temp_pipe_fd = pipeVec.back().first;
		cout<<pipeVec.back().first[0]<<endl;
        cout<<"toClientPipe: "<<toClientPipe<<" dupStdin: "<<dupStdin<<" dontDup: "<<dontDup<<endl;	
		cout<<"before fork()"<<endl;

		pid = fork();
		if(pid == 0){
			close(pipeVec.back().first[0]);
            if(toFile == true){
			    file_fd = open(rfilename,O_TRUNC|O_RDWR|O_CREAT,0777);
                cout<<"toFile dupping"<<endl;
                dup2(file_fd,1);					//direct the stdout to file
                dup2(pipeVec.back().first[1],2);		//direct the stderr 
                close(pipeVec.back().first[1]);
                check_dup_exec_vec(token,arg);
            }
            else if(toClientPipe==true && dupStdin==false){
                cerr<<"dup to client"<<endl;
                dup2(w_cli_fd,1);
                dup2(w_cli_fd,2);
                close(w_cli_fd);
                //close(temp_pipe_fd[1]);
                check_dup_exec_vec(token,arg);
            }
           else if(toClientPipe==false && dupStdin==true){
                cerr<<"dupStdin only: "<<endl;
                dup2(temp_pipe_fd[1],1);        //direct the stdout to pipe
                dup2(temp_pipe_fd[1],2);        //direct the stdout to pipe
                close(temp_pipe_fd[1]);
                check_dup_exec_vec(token,arg,r_cli_fd);
            }
            else if(toClientPipe==true && dupStdin==true){
                cerr<<"BOTH TRUE dup to client"<<endl;
                dup2(w_cli_fd,1);
                dup2(w_cli_fd,2);
                close(w_cli_fd);
                //close(temp_pipe_fd[1]);
                check_dup_exec_vec(token,arg,r_cli_fd);
            }
            else if(dontDup){
                cerr<<"not dupping"<<endl;
                check_dup_exec_vec(token, arg);
            }
            else{
                cerr<<"normal to pipe duping"<<endl;
                cerr<<"CHILD closing pipe [1]:    "<<temp_pipe_fd[1]<<endl;
                dup2(temp_pipe_fd[1],1);        //direct the stdout to pipe
                //dup2(temp_pipe_fd[1],2);        //direct the stderr 
                close(temp_pipe_fd[1]);
                //cerr<<"check_dup_exec start"<<endl;
                if(check_dup_exec_vec(token,arg)<0){
                    cerr<<"errorrr"<<endl;
                }
            } 
			exit(1);
		}else if(pid<0){ 
			perror("parse command: fork failed\n");
		}
		else{
            cout<<"parent()"<<endl;
            cout<<"toClientPipe: "<<toClientPipe<<" dupStdin: "<<dupStdin<<" dontDup: "<<dontDup<<endl;	
			close(pipeVec.back().first[1]);
			wait(&status);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
            if(status == 256){
                cerr<<"EXEC ERROR!!! "<<endl;
                cerr<<"STATUS: "<<status<<endl;
                sleep(5);
                pipeVec.pop_back();
                break;    
            }
            if(toFile){
                close(file_fd);
            }

            cout<<"writing to client"<<endl;
			if(pipeVec.back().second == -2){
				char buffer[BUFFER_SIZE];
				memset(buffer,0,BUFFER_SIZE);
				while(read(pipeVec.back().first[0],buffer, sizeof(buffer)) != 0){
                    string msg=(string)buffer + "\n";
                    if(toClientPipe){
                        cout<<"write to fifo:"<<endl;
                        fifo_write(msg,u_index+1, dest_id);
                        
                        break;    
                    }
					write(sockfd,msg.c_str(),msg.length());
				}
				close(pipeVec.back().first[0]);
				pipeVec.pop_back();
			}
            cout<<"after writing to client"<<endl;
            if(toClientPipe==true){
                my_shm_attach(shmid);
                cout<<"if toCli"<<endl;
                broadcast_mess(to_cli_msg_w);
                my_shm_deattach(shmid);
                //pipe has been closed by child    
            }
            if(dupStdin==true){
                my_shm_attach(shmid);
                cout<<"if dupStdin"<<endl;
                broadcast_mess(to_cli_msg_r);
                my_shm_deattach(shmid);
                //close(r_cli_fd);
            }
				
			remove_zero_vec();					//remove vec element whose counter = 0
			decrement_vec();					//-- each current element in pipe

			pcount++; 							//instruction counter
		}

	}
    cout<<"writing %"<<endl;
    write(sockfd,"% ",3);
    cout<<"writing % /DONE"<<endl;
}

int check_to_clientpipe_redir(char* temp){
    if(*temp == '>'){
        my_shm_attach(shmid);
        cout<<"to other client redirection found"<<endl;
        dest_id = atoi(temp+1);
        int from_id = all_user->id_arr[u_index];
        
        if(all_user->pid_arr[dest_id-1] == -1){
            //destid not found
            string str = "*** Error: user #"+to_string(dest_id)+" does not exist yet. ***\n% ";
            write(newsocketfd, str.c_str(),sizeof(char)*strlen(str.c_str()));
            dontDup=true;
            return -1;
        }
        else if(all_user->fifo_checklist[from_id][dest_id] == 1){
            //destid not found
            string str = "*** Error: the pipe #"+to_string(from_id)+"->#"+to_string(dest_id)+" already exists. ***\n% ";
            write(newsocketfd, str.c_str(),sizeof(char)*strlen(str.c_str()));
            dontDup=true;
            return -1;
        }
        else{
            toClientPipe=true;
            all_user->fifo_checklist[from_id][dest_id] = 1;       
            cout<<"fifo_checklist[][]: "<<all_user->fifo_checklist[from_id][dest_id]<<endl; 
            to_cli_msg_w = "*** "+(string)all_user->name_arr[u_index]+" (#"+to_string(from_id)+") just piped '"+(string)cmd+"' to "+(string)all_user->name_arr[dest_id-1]+" (#"+to_string(dest_id)+") ***\n";
            //dup the stdout directly to fifo
            string fifoname = fifo_path + to_string(from_id) +"_"+to_string(dest_id);
            w_cli_fd = open(fifoname.c_str(), O_NONBLOCK | O_WRONLY);
            if(w_cli_fd < 0){
                perror("open fifo"); return -1;
            }
        }
        return 1;
        my_shm_deattach(shmid);
    }
    else if(*temp == '<'){
        my_shm_attach(shmid);
        int sender_id = atoi(temp+1);
        int recv_id = u_index+1;
        cerr<<"getting a pipe from other client"<<endl;
        cout<<"fifo_checklist[][]: "<<all_user->fifo_checklist[sender_id][recv_id]<<endl; 
        if(all_user->fifo_checklist[sender_id][recv_id] == -1){
            string msg = "*** Error: the pipe #"+to_string(sender_id)+"->#"+to_string(recv_id)+" does not exist yet. ***\n% ";
            write(newsocketfd, msg.c_str(), msg.length());
            return -1;
        }
        else{
           cout<<"read from fifo and dup to stdin"<<endl;
           //char buff[BUFFER_SIZE];
           dupStdin=true;
           to_cli_msg_r = "*** "+(string)all_user->name_arr[u_index]+" (#"+to_string(all_user->id_arr[u_index])+") just received from "+(string)all_user->name_arr[sender_id-1]+" (#"+to_string(sender_id)+") by '"+(string)cmd+"' ***\n";
           //Dup the stdin directly from fifo later 
            //string fifoname = fifopath + to_string(sender_id) +"_"+to_string(recv_id);
            r_cli_fd = fifofd[sender_id][recv_id];
            if(r_cli_fd < 0){
                perror("open fifo"); return -1;
            }
            all_user->fifo_checklist[sender_id][recv_id]=-1;

        }
        return 1;
        my_shm_deattach(shmid);
    }
    return 0;    
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

int check_dup_exec_vec(char* token, char** arg,int ext_pipe_fd){
	int t_pipe[2];
    char buff[BUFFER_SIZE]={""};
	pipe(t_pipe);

	for(int i=0; i<pipeVec.size(); i++){
		if(pipeVec[i].second == 0){
			memset(buff,0,BUFFER_SIZE);
			read(pipeVec[i].first[0],buff,BUFFER_SIZE);
			write(t_pipe[1],buff,strlen(buff));
		}
    }	
    if(ext_pipe_fd != -1){
        memset(buff,0,BUFFER_SIZE);
        //cerr<<"read from cli pipe"<<ext_pipe_fd<<endl;
        int n = read(ext_pipe_fd,buff,BUFFER_SIZE);
        //cerr<<"r ret val: "<<n<<endl;
        //cerr<<"buff: "<<buff<<endl;
        n = write(t_pipe[1],buff,strlen(buff));
        //cerr<<"w ret val: "<<n<<endl;
        //close(ext_pipe_fd);
    }
	dup2(t_pipe[0],0);
	close(t_pipe[1]);

    int ret=0;
    ret =	exec_comm(token, arg);  

return ret;
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
    string valid_command[8]={"suicide","who","yell","tell","name","noop","printenv","setenv"};
    for(int it=0;it<8;it++){
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
		int n = execvp(arg[0],arg);
        if(n<0){
            perror("exec_comm: ");    
        }
	}
   

return -1;
}	

