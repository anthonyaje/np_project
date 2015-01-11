#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

using namespace std;
//DEFINE
#define MAX_CONN 5
#define PORT_NUM 8867 //dynamic allocation port 0
#define BUFFER_SIZE 1024
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

//FUNCTION PROTOTYPE
int init_socket(u_short *);
void process_request(int client);
int get_line(int sock, char* buf, int size);
void respond_unimplemented(int client);
//void respond_not_found(int client);
void execute_cgi(int client,const char* path,const char* method,const char* query_string);
void bad_request(int client);
void cannot_execute(int client);
void cat(int client, FILE *resource);
void not_found(int client);
void serve_file(int client, const char* filename);
void headers(int client, const char *filename);


 
int main(int argc, char** argv){
    int server_sock = -1;
    u_short port = PORT_NUM;
    int client_sock = -1;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    pthread_t newthread;

    server_sock = init_socket(&port); 
    printf("httpd running on port %d\n",port);
    while(1){
        client_sock = accept(server_sock,(struct sockaddr *) &client_addr,(socklen_t*) &client_addr_len);    
        if(client_sock == -1)
            perror("accept");
        process_request(client_sock);
     /*   if(pthread_create(&newthread, NULL, process_request, client_sock) != 0)
            perror("pthread create error");
     */
    }

    close(server_sock);

    return 0;
}

int init_socket(u_short* port){
    int httpd = 0;
    struct sockaddr_in addr;

    httpd = socket(AF_INET, SOCK_STREAM, 0);    
    if(httpd == -1){
        perror("socket");
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(httpd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        perror("bind");
    if(*port == 0){   //dynamic allocating port
        int addrlen = sizeof(addr);
        if(getsockname(httpd, (struct sockaddr *) &addr,(socklen_t*) &addrlen) == -1)
            perror("getsockname");
        *port = ntohs(addr.sin_port);
    }

    if(listen(httpd, MAX_CONN) < 0)
        perror("listen");

    return httpd;
}

void process_request(int client){
    char buffer[BUFFER_SIZE];
    char method[255];
    char url[255];
    char path[1024];
    bool cgi = false;
    int numchars;
    struct stat st;
    char* tmp, *query_string;
    
    numchars = get_line(client, buffer, BUFFER_SIZE);
    if(numchars < 0)
        perror("reading");
    /*while(numchars > 0){
        printf("%s\n",buffer);
        numchars = get_line(client, buffer, BUFFER_SIZE);
    }
    */

    printf("BUFFER: [%s]\n",buffer);
    tmp = strtok(buffer," \n");
    strcpy(method,tmp);
    printf("method: %s\n",method);
    tmp = strtok(NULL," \n");
    strcpy(url,tmp);
    if(strcasecmp(method,"GET") == 0){
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?'){
            cgi = true;
            *query_string = '\0';
            query_string++;
        } 
    }
    else if(strcasecmp(method,"POST") == 0){
       cgi = true; 
    }
    else{
        respond_unimplemented(client);
        return;    
    }

    //numchars = get_line(client, tmp, BUFFER_SIZE);
    //tmp = strtok(tmp," \n");
    //tmp = strtok(NULL," \n");
    //strcpy(path,tmp);
    strcpy(path,".");
    strcat(path,url);
    printf("PATH: %s\n",path);

    if(cgi){
        printf("cgi true! Executing CGI\n");
        cout<<"executing cgi"<<endl;
        execute_cgi(client, path, method, query_string);
        cout<<"executing cgi done"<<endl;
    }
    else{
        printf("Not Calling CGI\n");
        serve_file(client, path);
        printf("done serving file\n");
    }

    close(client);

}

int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        //DEBUG printf("%02X\n", c); 
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                //DEBUG printf("peek %02X\n", c); 
                if ((n > 0) && (c == '\n')){
                    recv(sock, &c, 1, 0);
                }
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

void execute_cgi(int client,const char* path,const char* method,const char* query_string){
    char buf[BUFFER_SIZE];
    int numchars = 1;
    int status;
    pid_t pid;
    int content_length = -1;
    int fd_cgi_out[2];
    int fd_cgi_in[2];
    char c;

    if(strcasecmp(method , "GET") == 0){
        while((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(client, buf, sizeof(buf));    
    }
    else{ /*POST*/
        numchars = get_line(client,buf, sizeof(buf));
        while((numchars > 0) && strcmp("\n", buf)){
            buf[15] = '\0';
            if(strcasecmp(buf,"Content-Length:") == 0)      //Get the length and discard the header
                content_length = atoi(&(buf[16]));
            numchars = get_line(client,buf, sizeof(buf));
        }
        if(content_length == -1){
            bad_request(client);
            return;    
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    
    pipe(fd_cgi_in);
    pipe(fd_cgi_out);
    cout<<"before fork()"<<endl;
    if((pid = fork()) < 0){
        cannot_execute(client);
        return;
    } 
    if(pid == 0){ //Child
        cout<<"child"<<endl;
        char method_env[255];
        char query_env[255];
        char length_env[255];

        dup2(fd_cgi_out[1],1);
        dup2(fd_cgi_in[0],0);
        close(fd_cgi_out[1]);
        close(fd_cgi_in[0]);
        sprintf(method_env, "REQUEST_METHOD=%s", method);
        putenv(method_env);
        if(strcasecmp(method,"GET") == 0){
            sprintf(query_env,"QUERY_STRING=%s",query_string);
            putenv(query_env);
        }
        else{    //POST
            sprintf(length_env,"CONTENT_LENGTH=%d",content_length);
            cerr<<length_env<<endl;
            putenv(length_env);
        }
        cerr<<"EXECUTING "<<path<<endl; 
        //execl("/~mjhung/study/mult.cgi","/~mjhung/study/mult.cgi",NULL);
        execl(path,path,NULL);


        exit(0);

    }else{ //Parent
        cout<<"parent"<<endl;
        close(fd_cgi_out[1]);
        close(fd_cgi_in[0]);
        if(strcasecmp(method,"POST") == 0)
            for(int i=0; i < content_length; i++){
                recv(client, &c, 1 ,0);
                //printf("DEBUG: [%c]\n",c);
                write(fd_cgi_in[1],&c,1);    
            }
        cout<<"Parent reading"<<endl;
        while(read(fd_cgi_out[0],&c,1) > 0){
            //printf("from cgi [%c]\n",c);
            send(client, &c, 1 ,0);
        }

        waitpid(pid,&status, 0);
        close(fd_cgi_in[1]);
        close(fd_cgi_out[0]);
    }

}

void serve_file(int client, const char* filename){
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));
    printf("openning file: [%s]\n",filename);
    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
        fclose(resource);        
    }
}


void respond_unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
