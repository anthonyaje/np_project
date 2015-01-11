#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <netdb.h>
#include <errno.h>
#include <algorithm>
#include <stdlib.h>
#include <arpa/inet.h>

#define LISTEN_PORT 9999 
#define MAX_CONN 5
#define BUFFER_SIZE 1024*100

using namespace std;

//PROTOTYPES
int check_socksconf(unsigned int ip);
int connectTCP(const char* host,const char* port);
int redirection_select(int ssockfd, int rsockfd);
int passiveTCP(int listen_port);


int main(int argc, char** argv){
    int listen_port, socketfd, ssockfd, cli_addr_size;
    struct sockaddr_in serv_addr, cli_addr;
    int child_pid; 

    listen_port = LISTEN_PORT;
    if(argc == 2)
        listen_port = atoi(argv[1]);
    
    if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("socks server: socket() error");    
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(listen_port);
    
    if(bind(socketfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
        perror("socks server: bind() error");    

    listen(socketfd, MAX_CONN);
    printf("listening at port %d\n",LISTEN_PORT);

    while(1){
        cli_addr_size = sizeof(cli_addr);
        cout<<"accepting"<<endl;
        ssockfd = accept(socketfd, (struct sockaddr*) &cli_addr, (socklen_t*) &cli_addr_size);
        cout<<"accepted: "<<ssockfd<<endl;
        if(ssockfd < 0){
            perror("socks server: accept() error");
            return 1;
        }

        if((child_pid = fork()) < 0){
            perror("fork() error");
        }
        else if(child_pid == 0){                    //Child process
            close(socketfd);
            cerr<<"inchild proces"<<endl;
            char buffer[BUFFER_SIZE];
            unsigned char package[BUFFER_SIZE];
            memset(buffer,0,BUFFER_SIZE);
            memset(package,0,BUFFER_SIZE);
            int n, deny_access, rep_CD, rsockfd;
            int done=0;

            while(done == 0){
                //reading data
                n = read(ssockfd, buffer, BUFFER_SIZE);
                unsigned char VN = buffer[0];
                unsigned char CD = buffer[1] ;
                unsigned int DST_PORT = buffer[2] << 8 | buffer[3] ;
                unsigned int DST_IP = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 |  buffer[7] ;
                char* USER_ID = buffer + 8 ;
                deny_access = check_socksconf(DST_IP);
                
                printf("Read Buffer: [%h] Buffer Len:[%d]\n",buffer,strlen(buffer));
                printf("Dest IP:[%u]. Dest Port:[%u]",DST_IP&0xFFFFFFFF, DST_PORT&0xFFFFFFFF);
                //init return package
                package[0] = 0;

                if(deny_access){
                    //access denied
                    package[1] = (unsigned char) 91 ; // 90 or 91 
                }
                else{
                    //CONNECT
                    if(CD == 1){
                        printf("**** Inside CONNECT\n");
                        package[1] = (unsigned char) 90 ; // 90 or 91
                        //copying IP and PORT 
                        for(int k=2; k<=7; k++)
                            package[k] = buffer[k];

                        for(int j=0;j<8;j++){
                            printf("pack[%d]: %u\n",j ,package[j]&0x000000FF);
                        }
                        string str_ip, str_port;
                        str_ip = "";
                        str_port = "";
                        //Bricking string ip & port
                        char tmp[100];
                        for(int i=4; i<7; i++){
                            snprintf(tmp,sizeof(tmp),"%u", buffer[i]&0x000000FF);
                            str_ip += (string)tmp + "."; 
                            memset(tmp,0,sizeof(tmp));
                        }
                        snprintf(tmp,sizeof(tmp),"%u", buffer[7]&0x000000FF);
                        str_ip += tmp;
                        unsigned int p =((unsigned int) buffer[2]&0x000000FF)*256 + ((unsigned int) buffer[3]&0x000000FF);
                        snprintf(tmp,sizeof(tmp),"%u",p);
                        str_port += tmp;
                        cout<<"STR IP:"<<str_ip<<" STR Port: "<<str_port<<endl;
                        
                        //Send the reply ti CGI
                        write(ssockfd,package, sizeof(package));
                        printf("reply package sent back!!\n");
                        rsockfd = connectTCP(str_ip.c_str(),str_port.c_str());
                        //Do redirection between cgi <-> socks <-> ras
                        redirection_select(ssockfd, rsockfd);
                        done = 1;
                    }
                    //BIND
                    else if(CD == 2){
                        printf("**** Inside BIND\n");
                        int psockfd;
                        //assume listening to port 211
                        int listen_port = 211;
                        int sockfd_in;
                        struct sockaddr_in cli_addr;
                        int cli_addr_size;
                        package[1] = (unsigned char) 90 ; // 90 or 91 
                        package[2] = listen_port / 256; 
                        package[3] = listen_port % 256; 
                        for(int k=4; k<=7; k++)
                            package[k] = 0;

                        psockfd = passiveTCP(listen_port); 
                        write(sockfd_in, package, sizeof(package));
                        sockfd_in = accept(psockfd, (struct sockaddr*) &cli_addr, (socklen_t*)&cli_addr_size);
                        if(sockfd_in < 0){
                            perror("(passive) accept error");
                        }else{
                            write(sockfd_in, package, sizeof(package));
                            //sleep(1);
                            redirection_select(ssockfd, sockfd_in);
                        }
                        
                        close(listen_port);
                    }else{
                        perror("ERROR CD unknown: not connect not bind");

                    }
                }
            }
        }
        else{                                       //Parent process
            close(ssockfd);       
            cerr<<"### in parent process\n\n"<<endl;

        }

    }


return 0;    
}


int redirection_select(int ssockfd, int rsockfd){
    fd_set read_set, active_set;
    int ndfs, redirect;
    FD_ZERO(&read_set);
    FD_ZERO(&active_set);
    FD_SET(rsockfd, &read_set);
    FD_SET(ssockfd, &read_set);
    ndfs = rsockfd>ssockfd?rsockfd:ssockfd;
    ndfs++;
    char* buffer[BUFFER_SIZE];
    int closeS, closeR;
    closeS = 0;
    closeR = 0;
    while(1){
        active_set = read_set;
        int nready = select(ndfs, &active_set, (fd_set*)0, (fd_set*)0, (struct timeval*)0 );
        if(nready<0){
            perror("select");
            break;
        }
        if(FD_ISSET(rsockfd, &active_set)){
            memset(buffer,0,BUFFER_SIZE);
            // sleep(1);
            int ret = read(rsockfd, buffer, BUFFER_SIZE);
            if(ret<0){
                perror("read error rsockfd");
                //done = 1;
                break;
            
            }else if(ret==0){
                perror("ret == 0");    
                FD_CLR(rsockfd,&active_set);
                closeR = 1;
                shutdown(rsockfd,SHUT_RD);
                shutdown(ssockfd,SHUT_RD);
                //printf("-> Buffer: \n[%s]\n",buffer);
                //write(rsockfd,buffer,sizeof(buffer));
                if(closeR&&closeS)
                    break;
            }
            else{
                printf("<- Buffer: \n[%s]\n",buffer);
                int r = write(ssockfd,buffer,sizeof(buffer));
                if(r<0)
                    perror("write error select");
            }
        }
        if(FD_ISSET(ssockfd, &active_set)){
            memset(buffer,0,BUFFER_SIZE);
            int ret = read(ssockfd, buffer, BUFFER_SIZE);
            if(ret<0){
                perror("read error ssockfd");
                //done = 1;
                break;
            }
            else if(ret==0){
                perror("ret == 0");    
                FD_CLR(ssockfd,&active_set);
                closeS = 1;
                shutdown(rsockfd,SHUT_RD);
                shutdown(ssockfd,SHUT_RD);
                //printf("-> Buffer: \n[%s]\n",buffer);
                //write(rsockfd,buffer,sizeof(buffer));
                if(closeR&&closeS)
                    break;
            }
            else{
                printf("-> Buffer: \n[%s]\n",buffer);
                int r = write(rsockfd,buffer,sizeof(buffer));
                if(r<0)
                    perror("write error select");
                //Need Sleep here 
                sleep(1);
            }
        }
    }
    close(rsockfd);
    close(ssockfd);
return 0;
}
int check_socksconf(unsigned int ip){
    

return 0;    
}

int passiveTCP(int listen_port){
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0) < 0)){
        perror("socker error passive");
    }
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(listen_port);

    if(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
        perror("socks server: bind() error");

    listen(sockfd,MAX_CONN);
    printf("now listening to port: %d\n",listen_port);

return sockfd;
}

int connectTCP(const char* host,const char* port){
    struct sockaddr_in serv_addr;
    int socketfd, status;
    
    if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket error: ");
        return 0;
    }

    memset(&serv_addr,'0',sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    if(inet_pton(AF_INET, host, &serv_addr.sin_addr) <=0){
        perror("inet_pton error");
    }
    int ret = connect(socketfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if(ret < 0) {
        perror("connect error: ");
        return 0;
    }

    return socketfd;
}




















