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
    vector<string> env_name;
    vector<string> env_addr;
};

class client_pipe{
    public:
    int from;
    int to;
    int pipefd[2];    
};

//vector<pair<int*,int> > pipeVec;
vector< pair<vector<pair<int*,int> >, int> >pipeVec_arr;
vector<user> user_list;
vector<client_pipe> cli_pipe_list; 
string cli_msg_w, cli_msg_r;

//PROTOTYPE
void err_dump(char* str){
	perror(str);
}
int print_ip(struct sockaddr_in* cli_addr);
void process_command(char* command, int sockfd);
int num_of_pipe(char* line);
void takeout_pipe_n(char* command, int* arr); //move the pipe number from command into arr
int exec_comm(char* token, char** arg);
void decrement_vec(int idx);
int check_dup_exec_vec(int v_idx,char* token, char** arg,int ext_pipe_fd=-1);
void remove_zero_vec(int idx);
int parse_line(char** ,char*);
int validate_command(char** arg,int sockfd);
void check_redirection(char** arg);

int remove_user(vector<user>& user_list, int fd, int* id_list);
int broadcast_mess(string& mess, vector<user>& list, int myfd,int notme,int welcome=0);
int process_chat_command(int argc, char** arg, int fd);
int request_id(int* arr);
int check_name_avail(string name);
int user_fd_to_index(int fd);
int user_id_to_fd(int id); 
int pipe_alr_exist(int from_id, int to_id);

int pipeVecArr_id_to_index(int id);
int envname_to_idx(int i, string str); 

void print_pipe_vec(){
    cout<<"print_pipe_vec"<<endl;
    cout<<"pipeVec_arr.size: "<<pipeVec_arr.size()<<endl;
    for(int i=0; i<pipeVec_arr.size();i++){
        cout<<"user id: "<<pipeVec_arr[i].second<<endl;
        cout<<"pipeVec_arr.first.size: "<<pipeVec_arr[i].first.size()<<endl;
        for(int j=0;j<pipeVec_arr[i].first.size(); j++){
            cout<<"pipe[0]: "<<pipeVec_arr[i].first[j].first[0]<<endl;    
            cout<<"pipe[0] count: "<<pipeVec_arr[i].first[j].second<<endl;    
        }    
    }    
}

/*
*****************************************
***  MAIN
***
*****************************************
*/
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
	nfds = getdtablesize();	

	for(;;){
        cout<<"user_list.size(): "<<user_list.size()<<endl;
        
        ready_set = read_set;
        //SELECT HERE
        //print_pipe_vec();
        cout<<"running select"<<endl;
		int nready = select(nfds,&ready_set,(fd_set*)0,(fd_set*)0,(struct timeval*)0);
        if(nready<0){ perror("Error in select\n"); }

		if(FD_ISSET(listenfd,&ready_set)){
			cli_addr_size = sizeof(cli_addr);
			if( (connfd = accept(listenfd,(struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size)) <0){
				perror("Error on accept");
				exit(1);
			}
//			char welcomenote [] = "****************************************\n** Welcome to the information server. **\n****************************************\n";

			printf("client connection accepted!\n");
            
            user u;        
            u.id = request_id(id_list);
            u.name = "(no name)";
            //u.ip = (string)(inet_ntoa(cli_addr.sin_addr));
            u.ip = "CGILAB";
            //u.port = to_string(ntohs(cli_addr.sin_port));
            u.port = "511";
            u.fd = connfd;
            u.env_name.push_back("PATH");
            //u.env_addr.push_back(""); 
            u.env_addr.push_back("bin:."); 
            setenv(u.env_name.back().c_str(),u.env_addr.back().c_str(),1);
            user_list.push_back(u);
            
            //Create pipeVec for client and append to list
            pipeVec_arr.push_back(*(new pair< vector<pair<int*,int> >, int >));
            pipeVec_arr.back().second = u.id;

			//n = write(connfd,welcomenote,sizeof(welcomenote));
			//if(n<0){ perror("Error writing to socket"); exit(1); }

            //Tell other about me
            string mydetail = "*** User '"+u.name+"' entered from "+u.ip+"/"+u.port+". ***\n";
            broadcast_mess(mydetail,user_list,connfd,0,1);
			FD_SET(connfd,&read_set);
            /*for(int f=3; f<nfds; ++f){
                if(f != listenfd && FD_ISSET(f,&read_set)){
                    write(f,mydetail.c_str(),strlen(mydetail.c_str()));
                }
            }*/
            //cout<<"sleeping....."<<endl;
            //usleep(300000);
            cerr<<"----------------- sending %-------------"<<endl;
            n = write(connfd,"% ",sizeof(char)*strlen(("i% ")));
            if(n<0){ perror("Error writing to socket"); exit(1);}
            cerr<<"-----------------done sending %-------------"<<endl;

            user_count++;
            //adjust the upperbound for the ready_set
            //if(connfd >= nfds)
            //    nfds = connfd+1;
		}
        cout<<"nfds "<<nfds<<endl;
        int nfds_cpy = nfds;
		for(int it_fd=0; it_fd<nfds_cpy; it_fd++){
            if(it_fd!=listenfd && FD_ISSET(it_fd,&ready_set)){
                cout<<"id_fd: "<<it_fd<<endl;
                bzero(buffer, BUFFER_SIZE);
                n = read(it_fd, buffer, BUFFER_SIZE);
                if(n<0){ perror("Error reading from socket"); exit(1);}

                //EXIT
                if(strncmp(buffer,"exit",4)==0){
                    //remove user from list
                    remove_user(user_list,it_fd, id_list);
                    cout<<"after remove user"<<endl;
                    user_count--;
                    
                    FD_CLR(it_fd,&read_set);
                    cerr<<"exit!!"<<endl;
                    break;
                    //continue; 
                }

                int savstdout = dup(1);
                int sv_err = dup(2); 
                //Setting env variables 
                int j = user_fd_to_index(it_fd); 
                for(int i=0; i<user_list[j].env_name.size(); i++){
                    setenv(user_list[j].env_name[i].c_str(),user_list[j].env_addr[i].c_str(),1);
                }

                printf("\n\n############## parse begin ####\n");
                process_command(buffer, it_fd);
                printf("############## parse done ####\n\n\n");

                fflush(stdout);
                dup2(savstdout,1);
                dup2(sv_err,2);
                close(sv_err);
                close(savstdout);

                bzero(buffer, BUFFER_SIZE);
            }
        }
    }

    return 0;
}


/*
 ****************************************
 ***  process_chat_command()
 ***
 *****************************************
 */
int process_chat_command(char* inst, int argc, char** arg,int fd){
    char* comm = strtok(inst," \r\n");
    char* rest = strtok(NULL,"\r\n");

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
        //for(int i=1;i<argc;i++){
        //    msg += " "+(string)arg[i];    
        //}
        msg+=rest;
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
            comm = strtok(rest," \r\n");
            rest = strtok(NULL,"\r\n");
            msg = "*** "+my_name+" told you ***: ";
            //for(int i=2;i<argc;i++){
            //    msg += " "+(string)arg[i];    
            //}
            msg += rest;
            msg += "\n";  
            if(write(to_fd,msg.c_str(),sizeof(char)*strlen(msg.c_str())) < 0)
                perror("Error writing to socket"); 
        }


    }
    else if(strcmp(arg[0],"name")==0){
        //check validity
        if(check_name_avail((string) arg[1])==0){
            string msg = "*** User '"+(string)arg[1]+"' already exists. ***\n";
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
        return -1;    
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

int broadcast_mess(string& mess, vector<user>& list,int myfd, int notme, int welcome){
    int n;
    if(notme){
        for(int i=0; i<list.size(); i++){
            if(list[i].fd != myfd){
                cerr<<"sending1 to "<<list[i].id<<endl;
                n = write(list[i].fd,(char*) mess.c_str(),sizeof(char)*strlen(mess.c_str()) );
                if(n<0){ perror("Error writing to socket"); }
            }
        }
    }
    else{
        for(int i=0; i<list.size(); i++){
            cerr<<"sending2 to "<<list[i].id<<endl;
            //usleep(300000);
           if(list[i].fd == myfd){
               string welcomenote = "****************************************\n** Welcome to the information server. **\n****************************************\n";
               string str;
               if(welcome==1){
                str = welcomenote + mess;
               }
               else{
                str = mess;    
               }
                n = write(list[i].fd,(char*) (str).c_str(),sizeof(char)*strlen(str.c_str()) );
                if(n<0){ perror("Error writing to socket"); }
                //close(list[i].fd);
                //n = write(list[i].fd,"% ",sizeof(char)*strlen("% "));
                //if(n<0){ perror("Error writing to socket"); exit(1);}
           }
           else{
                n = write(list[i].fd,(char*) (mess).c_str(),sizeof(char)*(strlen(mess.c_str())) );
                if(n<0){ perror("Error writing to socket"); }
           }
        }
    }

return 0;
}

int remove_user(vector<user>& user_list, int fd, int* id_list){
    for(int i=user_list.size()-1; i>=0; i--)
        if(user_list[i].fd == fd){
            int u_id = user_list[i].id;
            //BROADCAST
            string mess = "*** User '"+user_list[i].name+"' left. ***\n";
            write(fd,mess.c_str(),mess.length());
            //usleep(300000);
            //write(fd,"% ",3);
            close(fd);
            broadcast_mess(mess,user_list,fd,1);

            //removing pipe in cli_pipe_list
            for(int j=cli_pipe_list.size()-1; j>=0; j--){
                if(cli_pipe_list[j].to==u_id || cli_pipe_list[j].from==u_id){
                    close(cli_pipe_list[j].pipefd[0]);    
                    close(cli_pipe_list[j].pipefd[1]);
                    cli_pipe_list.erase(cli_pipe_list.begin()+j);    
                }
            }
            id_list[u_id] = 0;
            //string mess = "*** User '("+user_list[i].name+")' left. ***\n";
            //broadcast_mess(mess,user_list,fd,0);
            //remove entry in pipe table
            pipeVec_arr.erase(pipeVec_arr.begin()+pipeVecArr_id_to_index(u_id));
            user_list.erase(user_list.begin()+i);    
        }

    return 0;
}

int pipe_alr_exist(int from_id, int to_id){
    for(int i=0; i<cli_pipe_list.size();i++){
        if(cli_pipe_list[i].from == from_id){
            if(cli_pipe_list[i].to == to_id){
                return i;    
            }    
        }    
    }
    return -1;    
}

int pipeVecArr_id_to_index(int id){
    for(int i=0; i<pipeVec_arr.size(); i++){
        if(pipeVec_arr[i].second == id)
            return i;    
    }

return -1;    
}

int envname_to_idx(int i, string str){
    for(int j=0; j<user_list[i].env_name.size(); j++){
        if(str.compare(user_list[i].env_name[j])==0){
            return j;    
        }    
    }    

    return -1;
} 

/*
****************************************
***  process_command()
***
*****************************************
*/
void process_command(char* command,int sockfd){
	int status,n_pipe,pcount, n_inst;
	pid_t pid;
	char *token;
	int* n_arr;
	char** inst_arr;
	int fd,cli_fd0,cli_fd1,r_cli_fd0,r_cli_fd1;
    char command_cpy[10000];
    char* cmd;

    int my_index = user_fd_to_index(sockfd);
    int my_id = user_list[my_index].id;

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
	for(int i=0;i<n_inst;i++){
		cout<<"inst_arr[i]: "<<inst_arr[i]<<endl;
		cout<<"n_arr[i]: "<<n_arr[i]<<endl;
	}
    cmd = strtok(command_cpy,"\r\n\0");

	//for(int i=0; i<n_pipe; i++){
	for(int i=0; i<n_inst; i++){
		char* arg[100]={};
		token = strtok(inst_arr[i]," \n");
		int argc=0;
		bool toFile=false;
		bool toClientPipe=false;
        bool dontDup=false;
        bool dupStdin=false;
		char* rfilename=NULL;
		char* temp;
	    int pipe_index;
        string err_str,err_str2;
        bool flag1, flag2;
        	
		flag1 = flag2 = false;

        cout<<"NEW COMMAND"<<endl;
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
                if(*temp=='>'){
			        cerr<<"to other client redirection found"<<endl;
                    int dest_id = atoi((temp+1));
                    int u_index = user_fd_to_index(sockfd);  
                    int source_id = user_list[u_index].id;
                    int dest_fd = user_id_to_fd(dest_id);
                    int dest_index = user_fd_to_index(dest_fd);  
                    
                    cout<<"Dest id: "<<dest_id<<endl;
                    if(dest_fd == -1){
                        err_str = "*** Error: user #"+to_string(dest_id)+" does not exist yet. ***\n% ";
                        flag1 = true;
                        //write(sockfd, err_str.c_str(),sizeof(char)*strlen(err_str.c_str()));
                        dontDup = true;       
                        //return;
                        break;
                    }
                    else{
                        //add new pipe to cli_pipe_list if it hasnt existed yet
                        if(pipe_alr_exist(source_id,dest_id) != -1){
                            toClientPipe=false;   
                            dontDup = true;       
                            err_str = "*** Error: the pipe #"+to_string(source_id)+"->#"+to_string(dest_id)+" already exists. ***\n% ";
                            flag1=true;
                            //write(sockfd, str.c_str(),sizeof(char)*strlen(str.c_str()));
                            //return;
                            break;  //execute parse chat
                        }
                        else{
                            toClientPipe=true;
                            cli_msg_w = "*** "+user_list[u_index].name+" (#"+to_string(user_list[u_index].id)+") just piped '"+(string)cmd+"' to "+user_list[dest_index].name+" (#"+to_string(user_list[dest_index].id)+") ***\n";
                            //broadcast_mess(cli_msg_w,user_list,sockfd,0);
                            client_pipe cp;
                            cp.from = source_id;
                            cp.to = dest_id;
                            pipe(cp.pipefd);
                            cli_pipe_list.push_back(cp);
                            cli_fd0 = cp.pipefd[0];
                            cli_fd1 = cp.pipefd[1];
                            cerr<<source_id<<"  SEND PIPE TO: "<<dest_id<<"\t\t\t\t Clifd0:    "<<cli_fd0<<endl;
                            //close(cli_fd0);
                            //break;
                        }
                    }
                }
                else if(*temp=='<'){
                   cerr<<"getting a pipe from other client"<<endl;
                   int sender_id = atoi(temp+1);
                   int sender_fd = user_id_to_fd(sender_id);

                    //int u_index = user_fd_to_index(sockfd);  
                    //int u_id = user_list[u_index].id;
                    //int dest_fd = user_id_to_fd(dest_id);
                    //int dest_index = user_fd_to_index(dest_fd);  

                   if((sender_fd==-1) || (pipe_alr_exist(sender_id,my_id)==-1)){
                       err_str2 = "*** Error: the pipe #"+to_string(sender_id)+"->#"+to_string(my_id)+" does not exist yet. ***\n% "; 
                       flag2=true;
                       //write(sockfd, msg.c_str(),sizeof(char)*strlen(msg.c_str()));
                       //return;
                       break;
                   }
                   else{
                        //read from pipe and dup to stdin
                        cout<<"read from pipe and dup to stdin"<<endl;
                        char buff[BUFFER_SIZE];
                        pipe_index = pipe_alr_exist(sender_id,my_id);
                        int* tempfd = cli_pipe_list[pipe_index].pipefd;
                        r_cli_fd0 = tempfd[0];
                        r_cli_fd1 = tempfd[1];
                        close(r_cli_fd1);
                        dupStdin=true;
                        cli_msg_r = "*** "+user_list[my_index].name+" (#"+to_string(user_list[my_index].id)+") just received from "+user_list[user_fd_to_index(sender_fd)].name+" (#"+to_string(user_list[user_fd_to_index(sender_fd)].id)+") by '"+(string)cmd+"' ***\n";
                        //broadcast_mess(cli_msg_r,user_list,sockfd,0);
                        cerr<<my_id<<"  RECV PIPE FROM: "<<sender_id<<"\t\t\t\t Clifd0:    "<<r_cli_fd0<<endl;
                        cerr<<"pipeindex <: "<<pipe_index<<endl;
                        //break;
                   }

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
            cout<<"inside printenv"<<endl;
            int i = user_fd_to_index(sockfd);
            int j = envname_to_idx(i, (string)arg[1]); //envname->index()
            if(j!=-1){
                string msg = user_list[i].env_name[j] +"="+ user_list[i].env_addr[j]+"\n";
                cout<<"message: "<<msg<<endl;
                int n = write(sockfd,msg.c_str(),sizeof(char)*strlen(msg.c_str()));
                cout<<"write ret: "<<n<<endl;
            }
			break;
		}
		else if(strcmp(token,"setenv")==0){
			if(setenv(arg[1],arg[2],1) != 0){
				cerr<<"setenv un-successfully setted"<<endl;
			}
            int i = user_fd_to_index(sockfd);
            int j = envname_to_idx(i, (string)arg[1]); //envname->index()
            if(j==-1){
                user_list[i].env_name.push_back((string) arg[1]);
                user_list[i].env_addr.push_back((string) arg[2]);
            }
            else{
                user_list[i].env_name[j]= (string) arg[1];
                user_list[i].env_addr[j]= (string) arg[2];
            }
            //write(sockfd,"% ",3);    
			break;
		}

		if(validate_command(arg,sockfd)==-1){
			break;
		}

        cerr<<"before process chat command"<<endl;
        if(process_chat_command(command_cpy,argc,arg,sockfd)==0){
            //write(sockfd,"% ",3);
        }
        if(flag1 || flag2){
            write(sockfd, err_str.c_str(),sizeof(char)*strlen(err_str.c_str()));
            if(flag2){
                write(sockfd, err_str2.c_str(),sizeof(char)*strlen(err_str2.c_str()));
            }
            return ;
        }

        int pipelist_i = pipeVecArr_id_to_index(my_id);
        int* temp_pipe_fd;

		pipeVec_arr[pipelist_i].first.push_back(*(new pair<int*, int>));  	//push to pipe		
		pipeVec_arr[pipelist_i].first.back().first = new int[2];
		pipeVec_arr[pipelist_i].first.back().second = n_arr[pcount];
		pipe(pipeVec_arr[pipelist_i].first.back().first);
		cout<<"newly allocate pipE[0]:      "<<pipeVec_arr[pipelist_i].first.back().first[0]<<endl;
		cout<<"newly allocate pipe[1]:      "<<pipeVec_arr[pipelist_i].first.back().first[1]<<endl;

        temp_pipe_fd = pipeVec_arr[pipelist_i].first.back().first; 

		cout<<"before fork()"<<endl;
        cout<<"toClientPipe: "<<toClientPipe<<". dupStdin: "<<dupStdin<<endl;
		pid = fork();
		if(pid == 0){
            cout<<"CHILD closing pipe [0]:    "<<temp_pipe_fd[0]<<endl;
			close(temp_pipe_fd[0]);

			if(toFile == true){
			    fd = open(rfilename,O_TRUNC|O_RDWR|O_CREAT,0777);
			    dup2(fd,1);					//direct the stdout to file
			    dup2(temp_pipe_fd[1],2);		//direct the stderr 
			    close(temp_pipe_fd[1]);
			    check_dup_exec_vec(pipelist_i,token,arg);
			}
            else if(toClientPipe==true && dupStdin==false){
                cerr<<"dup to client"<<endl;
                dup2(cli_fd1,1);   
                dup2(cli_fd1,2);
                close(cli_fd1);   
			    //close(temp_pipe_fd[1]);
			    check_dup_exec_vec(pipelist_i,token,arg);
            }
            else if(toClientPipe==false && dupStdin==true){
                cerr<<"dupStdin only: "<<r_cli_fd0<<endl;
			    dup2(temp_pipe_fd[1],1);		//direct the stdout to pipe
			    dup2(temp_pipe_fd[1],2);		//direct the stdout to pipe
			    close(temp_pipe_fd[1]);
                check_dup_exec_vec(pipelist_i,token,arg,r_cli_fd0);
            }
            else if(toClientPipe==true && dupStdin==true){
                cerr<<"BOTH TRUE dup to client"<<endl;
                dup2(cli_fd1,1);   
                dup2(cli_fd1,2);
                close(cli_fd1);   
			    //close(temp_pipe_fd[1]);
                check_dup_exec_vec(pipelist_i,token,arg,r_cli_fd0);
            }
            else if(dontDup){
                cerr<<"not dupping"<<endl;
                check_dup_exec_vec(pipelist_i,token, arg);    
            }
            else{
                cerr<<"CHILD closing pipe [1]:    "<<temp_pipe_fd[1]<<endl;
			    dup2(temp_pipe_fd[1],1);		//direct the stdout to pipe
			    //dup2(temp_pipe_fd[1],2);		//direct the stderr 
			    close(temp_pipe_fd[1]);
                //cerr<<"check_dup_exec start"<<endl;
			    if(check_dup_exec_vec(pipelist_i,token,arg)<0){
                    cerr<<"errorrr"<<endl;    
                }
                //string err_message = "Unknown command: [" + (string)token + "].\n";
                //write(sockfd,err_message.c_str(),err_message.length());
			}
            
			exit(1);
		}else if(pid<0){ 
			perror("parse command: fork failed\n");
		}
		else{	//PARENT
            cerr<<"*** back1 to parent"<<endl;
			wait(&status);
			if(status == 1){
				perror("parent: child process terminated with an error\n");
			}
            cout<<"Child return status:         "<<status<<endl;
            cout<<"PARENT closing pipe [1]:    "<<temp_pipe_fd[1]<<endl;
			close(temp_pipe_fd[1]);

            cerr<<"*** back2 to parent"<<endl;
            if(status == 256){//exec error
                cerr<<"STATUS: "<<status<<endl;
				pipeVec_arr[pipelist_i].first.pop_back();
                print_pipe_vec();
                break;
            }
            if(toFile){
                close(fd);
            }
            if(toClientPipe==true){
                //close(cli_fd1);
            }
            if(dupStdin==true){
                cerr<<"pipe_index: "<<pipe_index<<endl;
                close(r_cli_fd0);
                cli_pipe_list.erase(cli_pipe_list.begin() + pipe_index);       
            }
            cerr<<"writing to client "<<endl;
			if(pipeVec_arr[pipelist_i].first.back().second == -2){
				char buffer[BUFFER_SIZE];
				memset(buffer,0,BUFFER_SIZE);
                int i=0;
				while(read(temp_pipe_fd[0],buffer, sizeof(buffer)) != 0){
                    cerr<<"send buffer: ["<<buffer<<"]"<<endl;
                    printf("buffer(int): %d\n",buffer[0]);
                    string msg = (string) buffer ;
					write(sockfd,msg.c_str(),msg.length());
                    if(i++ > 50)
                        break;
				}
                //write(sockfd,"% ",3);    
                cout<<"PARENT WRITING TO CLIENT closing pipe [0]:    "<<temp_pipe_fd[0]<<endl;
				close(temp_pipe_fd[0]);

				pipeVec_arr[pipelist_i].first.pop_back();
			}
            else{
            }
            cerr<<"DONE writing to client "<<endl;
            //write(sockfd,"% ",3);    

            if(toClientPipe==true){
                broadcast_mess(cli_msg_w,user_list,sockfd,0);
            }
            if(dupStdin==true){
                broadcast_mess(cli_msg_r,user_list,sockfd,0);
            }

            cerr<<"calling remove zero "<<endl;
            remove_zero_vec(pipelist_i);					//remove vec element whose counter = 0
            cerr<<"calling decrement "<<endl;
            decrement_vec(pipelist_i);					//-- each current element in pipe
            cout<<"Printe PIPE"<<endl;
            print_pipe_vec();
            /*
            int fdd[2];
            pipe(fdd);
            cout<<"CALEE new fdd[0]: "<<fdd[0]<<endl;
            cout<<"CALEE new fdd[1]: "<<fdd[1]<<endl;
            close(fdd[0]);
            close(fdd[1]);
            */
			pcount++; 							//instruction counter
		}

	}
    write(sockfd,"% ",3);    
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

void remove_zero_vec(int idx){
	for(int i=pipeVec_arr[idx].first.size()-1; i>=0; i--){
		if((*(pipeVec_arr[idx].first.begin()+i)).second == 0){	
            cout<<"PARENT REMOVE ZERO closing pipe [0]:    "<<(*(pipeVec_arr[idx].first.begin()+i)).first[0]<<endl;
			close((*(pipeVec_arr[idx].first.begin()+i)).first[0]);
			pipeVec_arr[idx].first.erase(pipeVec_arr[idx].first.begin()+i);
		}
	}

}

void decrement_vec(int idx){
	for(int i=0; i<pipeVec_arr[idx].first.size(); i++){
		--pipeVec_arr[idx].first[i].second;
	}	
}

int check_dup_exec_vec(int v_idx, char* token, char** arg, int ext_pipe_fd){
	int t_pipe[2];
    char buff[BUFFER_SIZE]={""};
    int pid;

	pipe(t_pipe);
    //cerr<<"CheckDup: before for loop"<<endl;
	for(int i=0; i<pipeVec_arr[v_idx].first.size(); i++){
		if(pipeVec_arr[v_idx].first[i].second == 0){
			memset(buff,0,BUFFER_SIZE);
			read(pipeVec_arr[v_idx].first[i].first[0],buff,BUFFER_SIZE);
            //cerr<<"checkdup: buffer: "<<buff<<endl;
			write(t_pipe[1],buff,strlen(buff));
            //cerr<<"CheckDup: counter == 0. read write. first[0]: "<<pipeVec_arr[v_idx].first[i].first[0]<<endl;
		}
	}
    ///cerr<<"CheckDup: after for loop"<<endl;
    if(ext_pipe_fd != -1){
        memset(buff,0,BUFFER_SIZE);
        //cerr<<cli_pipe_list.size()<<endl;
       // cerr<<"read from cli pipe"<<ext_pipe_fd<<endl;
        int n = read(ext_pipe_fd,buff,BUFFER_SIZE);    
        //cerr<<"r ret val: "<<n<<endl;
        n = write(t_pipe[1],buff,strlen(buff));
        //cerr<<"w ret val: "<<n<<endl;
        //close(ext_pipe_fd);
    }	
	dup2(t_pipe[0],0);
    close(t_pipe[1]);

 //   cerr<<"CheckDup: calling exec_comm"<<endl;
	int ret = 0; 
	ret = exec_comm(token, arg); 
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
        cerr<<"child1"<<endl;
        int ret=0;
        ret = exec_comm(arg[0],arg);
        cerr<<"exec return val:    "<<ret;
        exit(1);    
    }
    else if(ppid<0){
        return -1;   
        
    }
    else{
        cerr<<"parent1"<<endl;
        int status;
        wait(&status);
        cerr<<"parent done waiting"<<endl;
        if(status==256){
            string err_message = "Unknown command: [" + (string)arg[0] + "].\n";
            write(sockfd,err_message.c_str(),err_message.length());
            return -1;   
        }
        if(status==1){
            return -1;   
        }
    }
    cerr<<"finish forking()"<<endl;
//******************************************
/*
	string valid_command[12]={"who","yell","tell","name","noop","ls","cat","printenv","setenv","number","removetag","removetag0"};
	for(int it=0;it<12;it++){			
		if(strcmp(token,valid_command[it].c_str())==0){
			return 1;
		}
	}
*/
	return 0;
}

int exec_comm(char* token, char** arg){
    //cerr<<"arg[0]"<<arg[0]<<endl;
	//if(validate_command(arg[0]) == 0){
	//	return 0;
	//}	
	if(strcmp(token,"printenv")==0){
	   printf("%s \n",getenv(arg[1]));

	}else if(strcmp(token,"setenv")==0){
		int ret_val;
		if(setenv(arg[1],arg[2],1)==0){
			cerr<<"setenv successfully setted"<<endl;
		}
	}
    else{	
    //    cerr<<"execvp arg[0]"<<arg[0]<<endl;
        int n = execvp(arg[0],arg);
   //     cerr<<"execvp n: "<<n<<endl;
		if(n == -1){
            perror("exec_comm: ");    
        };
	}

return -1;
}	

