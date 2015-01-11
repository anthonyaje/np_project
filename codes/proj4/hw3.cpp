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
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;
//CONSTANTS
#define BUFF_SIZE 10240
#define F_CONNECTING 0
#define F_CONNECTED 1 
#define F_READING 2
#define F_WRITING 3
#define F_DONE 4
#define F_INIT -1

//TYPEDEFs
typedef int TARGET_STATUS;
typedef struct target{
  int id;
  string ip;
  string port;
  string test_file;
  struct addrinfo* server_info;  
  int socketfd;
  FILE *filefp;
  TARGET_STATUS status;
  
  string sh;
  string sp; 
  string rasip;
} TARGET;

//PROTOTYPEs
void parse_query(char* str, char** h, char** port, char** f, char** sh, char** sp);
char* encode_html(char* str);
char* decode_html(char* str);
void print_col_html_begin();
void print_col_html_end();
void print_inject_script(int id, char* msg);
void feed_line_to_server(TARGET targets, int i);
void cleanup();
bool socks_veri(int i, char* sh, char* sp);
int host_to_ip(char* hostname, string& it);


//GLOBALs
vector<TARGET> targets;
int h_block[5];

int main(){
    char *data, *h[5], *p[5], *f[5], *sh[5], *sp[5];
    char* tmp;
    bool block = false;
    char* BLOCKED_IP = (char*)"140.113.210.";
    printf("%s%c%c\n",
            "Content-Type:text/html;charset=iso-8859-1",13,10);
    printf("<TITLE>PROJECT 3</TITLE>\n");
    printf("<H3></H3>\n");
    //data = getenv("QUERY_STRING");
    data = (char*) "h1=nplinux4.cs.nctu.edu.tw&p1=8867&f1=test%2Ft1.txt&sh1=nplinux4.cs.nctu.edu.tw&sp1=9999&h2=&p2=&f2=&sh2=&sp2=&h3=&p3=&f3=&sh3=&sp3=&h4=&p4=&f4=&sh4=&sp4=&h5=&p5=&f5=&sh5=&sp5=";
    //data = (char*) "h1=nplinux4.cs.nctu.edu.tw&p1=8864&f1=test%2Ft1.txt&h2=nplinux3.cs.nctu.edu.tw&p2=8863&f2=test%2Ft2.txt&h3=nplinux2.cs.nctu.edu.tw&p3=8862&f3=test%2Ft3.txt&h4=&p4=&f4=&h5=&p5=&f5=";
    //data = (char*) "h1=nplinux4.cs.nctu.edu.tw&p1=8864&f1=test%2Ft1.txt&h2=nplinux3.cs.nctu.edu.tw&p2=8863&f2=test%2Ft2.txt&h3=nplinux2.cs.nctu.edu.tw&p3=8862&f3=test%2Ft3.txt&h4=nplinux4.cs.nctu.edu.tw&p4=8861&f4=test%2Ft3.txt&h5=&p5=&f5=";
    data = decode_html(data);
    printf("DATA: [%s]<br><br>",data);
    if(data == NULL)
        printf("<P>Error! Error in passing data from form to script."); 
    
    parse_query(data,h,p,f,sh,sp);
   /* for(int i=0; i<5; i++){
        printf("i: %d",i);    
        if(h[i]!= NULL){
            cout<<"h: "<<h[i]<<" p: "<<p[i]<<" f: "<<f[i]<<" sh: "<<sh[i]<<" sp: "<<sp[i]<<endl;   
        }
        else{
            printf(" null\n");    
        }
    }
   */
    fflush(stdout);
    
    for(int i=0; i<5; i++){
        block=false;
        if(h[i] != NULL){
            TARGET t;
            t.id = i;
            t.ip = h[i];
            t.port = p[i];
            t.sh = sh[i];
            t.sp = sp[i];
            string sip;
            host_to_ip(h[i],sip); 
            t.rasip = sip;
            block = socks_veri(i, sh[i], sp[i])==1?false:true;
            t.test_file = f[i];
            t.status = F_INIT;
            t.filefp = NULL;
            string s(h[i]);
            if(s.find(BLOCKED_IP) != string::npos){
                //print_inject_script(t.id, (char*)"Connection Blocked");
                fprintf(stderr,"blocking\n");
                h_block[i]=(t.id);
                block = true;    
            }
            if(block == false){
                targets.push_back(t);
            }
        }
    }
    fprintf(stderr,"before print col begin\n");
    print_col_html_begin();
    fprintf(stderr,"after print col begin\n");

    fd_set read_fdset;
    fd_set sock_fdset;
    struct addrinfo hints;
    int fdmax = getdtablesize();
    //int fdmax = 0;
    FD_ZERO(&read_fdset);
    FD_ZERO(&sock_fdset);
    memset(&hints,0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status;
    int connected = 0;
    
    for(int i=0; i<targets.size(); i++){
        if( (targets[i].filefp = fopen(targets[i].test_file.c_str(),"r")) == NULL){
            perror("open testfile");
            continue;    
        }
        
    }
     
    for(int i=0; i<targets.size(); i++){
        if(status = getaddrinfo(targets[i].sh.c_str(), targets[i].sp.c_str(), &hints, &targets[i].server_info)){
            fprintf(stderr, "getaddrinfo error: %s: %s\n",targets[i].ip.c_str(), gai_strerror(status));
            exit(1);
        }   
    }
    for(int i=0; i<targets.size(); i++){
        targets[i].socketfd = socket(targets[i].server_info->ai_family, targets[i].server_info->ai_socktype, targets[i].server_info->ai_protocol);
        if(targets[i].socketfd < 0){
            perror("socket error:");    
            continue;
        }
        int ret = connect(targets[i].socketfd, targets[i].server_info->ai_addr, targets[i].server_info ->ai_addrlen);
        if((ret<0) && (errno != EINPROGRESS)){
            perror("connect error:");
            targets[i].status = F_CONNECTING;
            continue;
        }else{
            fprintf(stderr, "#%d connected\n",i);
            connected++;
            targets[i].status = F_CONNECTED;    
            //Brick package for SOCKS
            char req[BUFF_SIZE];
            char res[BUFF_SIZE];
            req[0] =  4;
            req[1] =  1;  //CONNECT
            req[2] = (atoi(targets[i].port.c_str())) / 256;
            req[3] = (atoi(targets[i].port.c_str())) % 256;
            char tmp[1024];
            strcpy(tmp,targets[i].rasip.c_str());
            char* t = strtok(tmp, ".\r\n ");
            for(int j=4; j<=7; j++){
                req[j] = atoi(t);
                cout<<"t: "<<t<<endl;
                t = strtok(NULL,".\r\n ");
            }
            for(int j=0;j<8;j++){
                printf("req[%d]: %u\n",j ,req[j]&0x000000FF);
            }
            req[8]='\0';
            //Send request to SOCKS
            write(targets[i].socketfd, req, strlen(req));
            //Waiting for the reply from SOCKS
            read(targets[i].socketfd, res, BUFF_SIZE);
            for(int i=0;i<8; i++)
                printf("res i: [%u]\n",res[i]&0x000000FF);
        }
        //Set the fd and adjust the fdmax   
        FD_SET(targets[i].socketfd, &sock_fdset);
        if(targets[i].socketfd+1 > fdmax){
             fdmax = targets[i].socketfd+1;
        }
    }
    cout<<"before asynch\n"<<endl;
    sleep(2);
    //Set the asynch
    for(int i=0; i<targets.size(); i++){
        if(targets[i].socketfd > 0){
            int flags = fcntl(targets[i].socketfd, F_GETFL, 0);
            fcntl(targets[i].socketfd, F_SETFL, flags | O_NONBLOCK);    
        }
    }

    int break_flag = 0;
    while(1){
        //Reconnection
        for(int i = 0; i< targets.size(); i++){
            if(targets[i].status == F_CONNECTING){
                fprintf(stderr,"Reconnecting\n");
                int ret = connect(targets[i].socketfd, targets[i].server_info->ai_addr, targets[i].server_info ->ai_addrlen);
                if((ret<0) && (errno != EINPROGRESS)){
                    perror("connect error:");
                    targets[i].status = F_CONNECTING;
                    continue;
                }
                else{
                    targets[i].status = F_CONNECTED;
                    connected++;
                    fprintf(stderr,"Resetting up fd: %d\n",targets[i].socketfd);
                    FD_SET(targets[i].socketfd, &sock_fdset);
                    //printf("Isset: %d\n",(FD_ISSET(targets[i].socketfd,&sock_fdset)));
                }    
            }
        }
                    
        if(connected == 0){
            sleep(1);
            continue;    
        }

        char buffer[BUFF_SIZE];
        int nbytes;

        read_fdset = sock_fdset;
        for(int i=0; i<targets.size(); i++){
            if(targets[i].status != F_INIT){
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                printf("Before SELECT of %d\n",i);
                //if(select(fdmax, &read_fdset, NULL, (fd_set*)0, (struct timeval*)0) < 0){
                if(select(fdmax, &read_fdset, NULL, (fd_set*)0, &tv) < 0){
                    perror("select");
                    continue;
                }
                printf("After SELECT of %d\n",i);
                fprintf(stderr,"status %d. sockfd: %d. is_set_val: %d.\n",targets[i].status, targets[i].socketfd,(FD_ISSET(targets[i].socketfd,&read_fdset)));
                if((targets[i].status==F_CONNECTED) && (FD_ISSET(targets[i].socketfd,&read_fdset))){
                    int sockfd = targets[i].socketfd;
                    memset(buffer, 0, BUFF_SIZE);
                    if((nbytes = recv(sockfd,buffer,BUFF_SIZE,0)) <= 0){
                        if(nbytes == 0){
                            fprintf(stderr,"connection closed by server\n");
                        }
                        else{
                            perror("recv:");    
                        }
                        close(sockfd);
                        connected--;
                        FD_CLR(sockfd,&sock_fdset);
                        targets.erase(targets.begin()+i);
                        if(targets.size() == 0){
                            fprintf(stderr,"Exit now, all connection to be killed");
                            cleanup();
                            return 1;
                        }
                        if(connected==0){
                            break_flag = 1;
                            break;
                        }
                    } 
                    else{
                        fprintf(stderr,"get response from #%d:%lu bytes:\n[%s]\n\n",i,strlen(buffer), buffer);
                        string s(buffer);
                        
                        char* t = strtok(buffer,"%\r\n");
                        while(t != NULL){
                            print_inject_script(targets[i].id, t);
                            t = strtok(NULL,"%\r\n");
                        }
                        if(s.find("%") != string::npos){
                            fprintf(stderr,"get prompt, one line server\n");
                            fprintf(stderr,"sockefd: %d\n",targets[i].socketfd);
                            print_inject_script(targets[i].id, (char*)"% ");
                            feed_line_to_server(targets[i],i);
                        }
                        //FIXME why it doesnt work if it is taken out
                        //sleep(1);
                    }
                }else{
                    fprintf(stderr,"either status not connected OR fd_isset\n");    
                }    
                //FIXME is this necessary 
                //read_fdset = sock_fdset;
            }
            if(break_flag){
                break;   
            }
            
        }    
    }
        
    return 0;    
}

int host_to_ip(char* hostname, string& sip){
    struct hostent *he;
    struct in_addr **addr_list;
    if ( (he = gethostbyname( hostname ) ) == NULL) 
    {
        // get the host info
        herror("gethostbyname");
        return 1;
    }
    addr_list = (struct in_addr **) he->h_addr_list;
    for(int i = 0; addr_list[i] != NULL; i++) 
    {
        //Return the first one;
        sip = inet_ntoa(*addr_list[i]);
        return 0;
    }
    return 1;    
}

bool socks_veri(int i, char* sh, char* sp){
//TODO

return 1;    
}
void cleanup(){
    for(int i=0; i<targets.size(); i++){
        if( fclose(targets[i].filefp)){
            perror("close testfile");
            continue;    
        }
    }
}

void print_inject_script(int id, char* msg){
    //FIXME encoding error
    //msg = encode_html(msg);
    //fprintf(stderr,"inside print_col_html_begin(). id:[%d] msg:\n[%s]\n", id, msg);
    char outbuf[300000];
    if(msg[strlen(msg)-1] == '\n' || msg[strlen(msg)-1] == '\r') 
        msg[strlen(msg)-1] = '\0';
    const char* templatee = "<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n";
    const char* template_withoutbr = "<script>document.all['m%d'].innerHTML += \"%s\";</script>\n";
    int i;
    if(strcmp(msg, "% ")==0)
    {
        sprintf(outbuf, template_withoutbr, id, msg);
    //    fprintf(stderr,"OUTBUF: %s\n",outbuf);
    }
    else
    {
        sprintf(outbuf, templatee, id, msg);
     //   fprintf(stderr,"%s\n",outbuf);
    }

    printf("%s", outbuf);
    fflush(stdout);       
}

void feed_line_to_server(TARGET depricated, int i){
    FILE * fp = targets[i].filefp; // = fopen(targets[i].test_file.c_str(),"r");
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    if(fp == NULL){
        perror("opening file");
        return;
    }
    //while ((read = getline(&line, &len, fp)) != -1) {
    read = getline(&line, &len, fp);
    fprintf(stderr,"FEED line: [%s] to [%d]\n", line,targets[i].socketfd);
    //line[len] = '\0';
    if(write(targets[i].socketfd, line, len+1) < 0){
        perror("feed_line write failed");    
        return;
    }
    char* t = strtok(line,"%\r\n");
    while(t != NULL){
        print_inject_script(targets[i].id, t);
        t = strtok(NULL,"%\r\n");
    }
    //fclose(fp);
    if (line)
        free(line);
}

void print_col_html_begin(){
    printf("Content-Type: text/html\n\n");
    printf("<html>\n");
    printf("<head>\n");
    printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
    printf("<title>Network Programming Homework 3</title>\n");
    printf("</head>\n");
    printf("<body bgcolor=#336699>\n");
    printf("<font face=\"Courier New\" size=2 color=#FFFF99>\n");
    printf("<table width=\"800\" border=\"1\">\n");
    printf("<tr>\n");
    for(int i = 0; i < targets.size(); i++){ 
        //printf("<td>%s</td>\n", targets[i].ip.c_str());
        printf("<td>%s</td>\n", targets[i].ip.c_str());
        //fprintf(stderr,"taarget: %d\n",targets[i].id);
    }

    //for(int i = 0; i < targets.size(); i++) printf("<td>%s</td>\n", targets[targets[i].id].ip.c_str());
    printf("<tr>\n");
    printf("<td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"></td><td valign=\"top\" id=\"m3\"></td><td valign=\"top\" id=\"m4\"></td></tr>\n");
    printf("</table>\n");    
    
}

void print_col_html_end(){
    printf("</font>\n");
    printf("</body>\n");
    printf("</html>\n");
}


char* decode_html(char* str){
    char* res = new char[BUFF_SIZE];
    string result = "";
    char* p;
    p = str;
    
    while((*p) != '\0'){
        if(*p == '%'){
            if(*(++p) == '2')
                if(*(++p) == 'F')
                    result += "/";
        }
        else
            result += *p;
        p++;
    }
    
    strcpy(res, result.c_str());

return res;
}

char* encode_html(char* str){
    char* res = new char[BUFF_SIZE];
    string result = "";
    char* p;
    p = str;
   
    cout<<"p: "<<p<<endl;
    while((*p) != '\0'){
        cout<<*p<<" ";
        if((int)*p >= 128)
            result += "&#"+*p;
        else if(*p == '<')
            result += "&lt;";
        else if(*p == '>')
            result += "&gt;";
        else if(*p == '&')
            result += "&amp;";
        else if(*p == '"')
            result += "&quot;";
        else if(*p == '\'')
            result += "&apos;";
        else
            result += *p;
        p++;        
    }
    strcpy(res, result.c_str());
    return res;
}

void parse_query(char* str, char** h, char** port, char** f, char** sh, char** sp){
    char* ptr;
    int i=0;
    int len = strlen(str);
    int top=0;
    char *p, *q;
    int NUM_COL=5;

    p = q = str;

    while(1){
        while(*p != '='){
            p++;    
        }
        p++;
        q = p;
        while((*q != '&') && (*q != '\0'))
            q++;
        if((q-p) <= 0){
            if(i % NUM_COL== 0)
                h[top] = NULL;     
            if(i % NUM_COL== 1)
                port[top] = NULL;     
            if(i % NUM_COL== 2)
                f[top] = NULL;     
            if(i % NUM_COL== 3)
                sh[top] = NULL;     
            if(i % NUM_COL== 4)
                sp[top++] = NULL;     
        }
        else{
            char* t = new char [1024];
            strncpy(t, p, q-p);
            cout<<"t: "<<t<<endl;
            cout<<"i: "<<i<<endl;
            if(i % NUM_COL ==0)
                h[top] = t;     
            if(i%NUM_COL ==1)
                port[top] = t;     
            if(i%NUM_COL ==2)
                f[top] = t;     
            if(i % NUM_COL== 3)
                sh[top] = t;     
            if(i % NUM_COL== 4)
                sp[top++] = t;     
        }
        i++;
        if(*q != '\0'){
            q++;
            p = q;
        }else{
            break;    
        }
    }
               
}

