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

            //while(done == 0){
                //reading data
                n = read(ssockfd, buffer, BUFFER_SIZE);
                unsigned char VN = buffer[0];
                unsigned char CD = buffer[1] ;
                unsigned int DST_PORT = buffer[2] << 8 | buffer[3] ;
                unsigned int DST_IP = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 |  buffer[7] ;
                char* USER_ID = buffer + 8 ;
                deny_access = check_socksconf(DST_IP);
                
                printf("Read Buffer: [%h] Buffer Len:[%d]\n",buffer,strlen(buffer));
                printf("VN: %d\n",VN);
                printf("Dest IP:[%u]. Dest Port:[%u]",DST_IP&0xFFFFFFFF, DST_PORT&0xFFFFFFFF);
                //init return package
                package[0] = 0;

                if(deny_access){
                    //access denied
                    package[1] = (unsigned char) 91 ; // 90 or 91 
                    for(int k=2; k<=7; k++)
                        package[k] = buffer[k];
                    write(ssockfd,package, sizeof(package));
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
                        
                        //Bricking string ip & port
                        string str_ip, str_port;
                        str_ip = "";
                        str_port = "";
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
                        
                        //Connect to DEST
                        rsockfd = connectTCP(str_ip.c_str(),str_port.c_str());
                        if(rsockfd<=0)
                            package[1] = (unsigned char) 91 ; // 90 or 91 
                        
                        //Send the reply ti CGI
                        write(ssockfd,package, sizeof(package));
                        printf("reply package sent back!!\n");
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
           // }
        }
        else{    //Parent process
            close(ssockfd);       
            cerr<<"### in parent process\n\n"<<endl;

        }

    }


return 0;    
}


int redirection_select(int ssock, int rsock){
    int nfds, n, fd, rc, i;
    int packet_to_server=0, packet_to_client=0;
    int closeR=0, closeS=0;
    fd_set rfds; //read file descriptor set
    fd_set afds; //active file descriptor set
    char buffer[BUFFER_SIZE];

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(ssock, &afds);
    FD_SET(rsock, &afds);

    while(1)
    {
        memcpy(&rfds, &afds, sizeof(rfds));

        if(select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0)<0)
        {
            printf("select error\n");
            return -1;
        }

        if(FD_ISSET(ssock, &rfds))
        {
            n = recv(ssock, buffer, sizeof(buffer), 0);
            if(n<0)
            {
                exit(EXIT_FAILURE);
            }
            else if(n==0)
            {
                printf("%d:\tEnd of ssock\n", getpid());
                closeS = 1;
                FD_CLR(ssock, &rfds);
                shutdown(ssock, SHUT_RD);
                shutdown(rsock, SHUT_RD);
                if(closeS&&closeR)
                {
                    break;
                }
            }
            else
            {
                rc = send(rsock, buffer, n, 0);
                if(rc<0)
                {
                    exit(EXIT_FAILURE);
                }
                else
                {
                    if(!packet_to_server)
                    {
                        printf("%d:\tredirect to remote server\n", getpid());
                        for(i = 0; i<((rc>40)?40:rc); i++)
                        {
                            if(buffer[i]>=32&&buffer[i]<=126)
                            {
                                printf("%c", buffer[i]);
                            }
                            else
                            {
                                printf("(%d)", buffer[i]);
                            }
                        }
                        if(rc>40)
                        {
                            printf("......\n");
                        }
                        else
                        {
                            printf("\n");
                        }

                        fflush(stdout);
                        packet_to_server = 1;
                    }
                }
            }
        }

        if(FD_ISSET(rsock, &rfds))
        {
            n = recv(rsock, buffer, sizeof(buffer), 0);
            if(n<0)
            {
                exit(EXIT_FAILURE);
            }
            else if(n==0)
            {
                printf("%d:\tEnd of rsock\n", getpid());
                closeR = 1;
                FD_CLR(rsock, &rfds);
                shutdown(rsock, SHUT_RD);
                shutdown(ssock, SHUT_RD);
                if(closeS&&closeR)
                {
                    break;
                }
            }
            else
            {
                rc = send(ssock, buffer, n, 0);
                if(rc<0)
                {
                    exit(EXIT_FAILURE);
                }
                else
                {
                    if(!packet_to_client)
                    {
                        printf("%d:\tredirect to client\n", getpid());
                        for(i = 0; i<((rc>50)?50:rc); i++)
                        {
                            if(buffer[i]>=32&&buffer[i]<=126)
                            {
                                printf("%c", buffer[i]);
                            }
                            else
                            {
                                printf("(%d)", buffer[i]);
                            }
                        }
                        if(rc>40)
                        {
                            printf("......\n");
                        }
                        else
                        {
                            printf("\n");
                        }

                        fflush(stdout);
                        packet_to_client = 1;
                    }

                }
            }
        }

    }

    close(ssock);
    close(rsock);

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




















