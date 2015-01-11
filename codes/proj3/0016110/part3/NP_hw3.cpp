//#include <windows.h>
#include <list>
#include "resource.h"
#include <string.h>
//#include <strings.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <fstream>

#include <winsock2.h>
#include <ws2tcpip.h>
using namespace std;

//DEFINE
#define SERVER_PORT 8867
#define BUFFER_SIZE 102400
#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define WM_SOCKET (WM_SOCKET_NOTIFY + 1)
#define F_CONNECTING 0
#define F_CONNECTED 1
#define F_READING 2
#define F_WRITING 3
#define F_DONE 4
#define F_INIT -1


typedef int TARGET_STATUS;
typedef struct target{
	int id;
	string ip;
	string port;
	string test_file;
	//struct addrinfo sa;
	SOCKADDR_IN sa;
	SOCKET socketfd;
	FILE* filefp;
	TARGET_STATUS status;
} TARGET;

//PROTOTYPE
BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
int process_request(SOCKET client);
int serve_file(SOCKET client, const char* filename);
int execute_cgi(SOCKET client, char* query_string);
int get_line(SOCKET sock, char *buf, int size);
char* decode_html(char* str);
void parse_query(char* str, char** h, char** p, char** f);
void print_col_html_begin(SOCKET client);
void print_col_html_end(SOCKET client);
void send_inject_script(SOCKET browser, int id, char* msg);
void feed_line_to_server(int i);

//	Global Variables
list<SOCKET> Socks;
static HWND hwndEdit;
vector<TARGET> targets;
SOCKET bro_sock;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	//static HWND hwndEdit;
	static SOCKET msock, ssock;
	static struct sockaddr_in sa;
	int err, ret;
	char buffer[BUFFER_SIZE];
	memset(buffer,0,sizeof(buffer));

	switch(Message) 
	{
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_LISTEN:

					WSAStartup(MAKEWORD(2, 0), &wsaData);

					//create master socket
					msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					if( msock == INVALID_SOCKET ) {
						EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
						WSACleanup();
						return TRUE;
					}

					err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

					if ( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(msock);
						WSACleanup();
						return TRUE;
					}

					//fill the address info about server
					sa.sin_family		= AF_INET;
					sa.sin_port			= htons(SERVER_PORT);
					sa.sin_addr.s_addr	= INADDR_ANY;

					//bind socket
					cerr<<"binding"<<endl;
					err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

					if( err == SOCKET_ERROR ) {

						EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
						WSACleanup();
						return FALSE;
					}

					err = listen(msock, 2);
		
					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
						WSACleanup();
						return FALSE;
					}
					else {
						EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
					}

					break;
				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_SOCKET_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) )
			{
				case FD_ACCEPT:
					ssock = accept(msock, NULL, NULL);
					Socks.push_back(ssock);
					EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
					bro_sock = ssock;
					break;
				case FD_READ:
					//Write your code for read event here.
					EditPrintf(hwndEdit, TEXT("=== ssock: %d, wParam: %d ===\r\n"), ssock, wParam);
					ret = process_request(wParam);
					//DONE: until for loop target[i].socketfd = socket()
					for(int i=0; i<targets.size();i++){
						ret = connect(targets[i].socketfd,(struct sockaddr*) &(targets[i].sa), sizeof(targets[i].sa));
						if((ret<0) ){
							EditPrintf(hwndEdit, TEXT("=== Error: last error: %d===\r\n"),GetLastError());
							targets[i].status = F_CONNECTING;
							//closesocket(targets[i].socketfd);
							//targets[i].socketfd = INVALID_SOCKET;
						}else{
							targets[i].status = F_CONNECTED;
						}

						ret = WSAAsyncSelect(targets[i].socketfd,hwnd, WM_SOCKET,FD_READ|FD_CLOSE);
						if ( ret == SOCKET_ERROR ) {
							EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
							closesocket(targets[i].socketfd);
							WSACleanup();
							return TRUE;
						}
					}
					break;
				case FD_WRITE:
					//Write your code for write event here

					break;
				case FD_CLOSE:
					EditPrintf(hwndEdit, TEXT("===Closing socket: %d\n"),wParam);
								closesocket(wParam);
								//WSACleanup();
					break;
			};
			break;
		
			case WM_SOCKET:
				EditPrintf(hwndEdit, TEXT("=== New event handler1 ===\r\n"));
				switch( WSAGETSELECTEVENT(lParam) )
				{
					EditPrintf(hwndEdit, TEXT("=== New event handler2 ===\r\n"));	
					case FD_READ:
						memset(buffer,0,strlen(buffer));
						if((ret = recv(wParam,buffer, BUFFER_SIZE,0)) <= 0 ){
							if(ret==0){
								EditPrintf(hwndEdit, TEXT("=== Connection closed by server ===\r\n"));
							}else{
								EditPrintf(hwndEdit, TEXT("=== Error: recv error %d===\r\n"),GetLastError());		
								closesocket(wParam);
								for(int i=targets.size()-1; i>=0; i--){
									if(targets[i].socketfd == wParam)
										targets.erase(targets.begin()+i);
								}
							}
							
							if(targets.size() == 0){
								EditPrintf(hwndEdit, TEXT("=== Exit now, all connection to be killed===\r\n"));
								EditPrintf(hwndEdit, TEXT("===Closing socket: %d\n"),wParam);
								closesocket(wParam);
								print_col_html_end(bro_sock);
								//WSACleanup();
								//return 1;
							}
						}
						else{
							EditPrintf(hwndEdit, TEXT("=== Getting response from server\r\n"));
							//EditPrintf(hwndEdit, TEXT("=== BUffer %s===\r\n"),buffer);		
							string s(buffer);
							int idx;
							for(int i=targets.size()-1; i>=0; i--){
								if(targets[i].socketfd == wParam)
									idx = targets[i].id;
							}
							char* t = strtok(buffer,"%\r\n");
							while((t!=NULL) && ((*t !=' ') && (strlen(t)!=1)) ){
							//while((t!=NULL)){
								send_inject_script(bro_sock,idx,t);
								t = strtok(NULL,"%\r\n");
							}
							if(s.find("%") != string::npos){
								send_inject_script(bro_sock,idx,(char*)"% ");
								feed_line_to_server(idx);
							}
							EditPrintf(hwndEdit, TEXT("=== Finishing command\r\n"));
							//Sleep(1000);
						}

						break;
					case FD_CLOSE:
								EditPrintf(hwndEdit, TEXT("===Closing socket: %d\n"),wParam);
								closesocket(wParam);
								//WSACleanup();
						break;
				}
			break;

		default:
			return FALSE;


	};

	return TRUE;
}

int process_request(SOCKET client){
	char buffer[BUFFER_SIZE];
    char method[255];
    char url[255];
    char path[1024];
    bool cgi = false;
    int numchars;
    struct stat st;
    char* tmp, *query_string;

//	recv(client,str,BUFFER_SIZE,0);
	
	numchars = get_line(client, buffer, BUFFER_SIZE);
	if(numchars > 0){
		//EditPrintf(hwndEdit, TEXT("===Bytes received: %d\n BUFFER: [%s]\n"),numchars, buffer);
		tmp = strtok(buffer," \n");
		strcpy(method,tmp);
		tmp = strtok(NULL," \n");
		//FIXME
		strcpy(url,tmp);
		if(strcmp(method,"GET") == 0){
			query_string = url;
			while ((*query_string != '?') && (*query_string != '\0'))
				query_string++;
			if (*query_string == '?'){
				cgi = true;
				*query_string = '\0';
				query_string++;
			}
		}
		else{
			EditPrintf(hwndEdit, TEXT("===Method unimplemented: BUFF: %s\n "),buffer);
			if(strcmp(method,"POST") == 0){
			   cgi = true;
			   //fprintf(stderr,"POST method used \n", numchars);
			}
			return -1;
		}
		strcpy(path,".");
		strcpy(path+1,url);
		EditPrintf(hwndEdit, TEXT("===files: %s\n"),path);
		if(cgi){
			//printf("cgi true! Executing CGI\n");
			execute_cgi(client, query_string);
			//closesocket(client);
		}
		else{
			serve_file(client, path);
			EditPrintf(hwndEdit, TEXT("===Closing socket: %d\n"),client);
			closesocket(client);
			//WSACleanup();
		}
		recv(client,buffer,BUFFER_SIZE,0);
		memset(buffer,0,BUFFER_SIZE);
	}else if(numchars == 0){
		//EditPrintf(hwndEdit, TEXT("===Numchars == 0\n"));
		//recv(client,buffer,BUFFER_SIZE,0);
		//memset(buffer,0,BUFFER_SIZE);
	}else{
		//fprintf(stderr,"recv failed wth error: %d\n", WSAGetLastError());
		closesocket(client);
		//WSACleanup();
		return 1;
	}
	return 0;
}

int execute_cgi(SOCKET client, char* query_string){
	char* data;
	char *h[5], *p[5], *f[5], *tmp;
	char str[BUFFER_SIZE];
	int numchars;
/*	numchars = get_line(client, str, BUFFER_SIZE);
	while((numchars>0)&&(strcmp("\n",str))){                //Eat the rest of the header
			numchars = get_line(client, str, BUFFER_SIZE);
	}
*/
	while(recv(client,str,BUFFER_SIZE,0) > 0){
		memset(str,0,BUFFER_SIZE);
	}
	
	
	string buffer("HTTP/1.0 200 OK\r\n");
	send(client, buffer.c_str(), buffer.length(), 0);
	buffer = "Cache-Control: public\r\n"; 
	send(client, buffer.c_str(), buffer.length(), 0);
	sprintf(str,"%s%c%c","Content-Type:text/html;charset=iso-8859-1",13,10);
	send(client,str, strlen(str),0);
    sprintf(str,"<TITLE>PROJECT 3</TITLE>\n");
	send(client,str, strlen(str),0);
    sprintf(str,"<H3></H3>\n");
	send(client,str, strlen(str),0);
	data = decode_html(query_string);
	if(data == NULL){
		EditPrintf(hwndEdit, TEXT("Data == Null\n"));
	}

	parse_query(data,h,p,f);
	for(int i=0; i<5; i++){
		if(h[i] != NULL){
			TARGET t;
            t.id = i;
            t.ip = h[i];
            t.port = p[i];
            t.test_file = f[i];
            t.status = F_INIT;
            t.filefp = NULL;
            targets.push_back(t);		
		}
	}
    print_col_html_begin(client);

	//struct addrinfo hints, *result=NULL, *ptr=NULL;
	WSADATA wsaData;
	int iResult;

	//memset(&hints,0, sizeof hints);
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	//hints.ai_family = AF_UNSPEC;
	//hints.ai_socktype = SOCK_STREAM;
	struct hostent *he;
	for(int i=0; i<targets.size(); i++){
		if( targets[i].status != F_CONNECTED ){
			if((he = gethostbyname(targets[i].ip.c_str())) == NULL ){
				//perror
				continue;
			}	
			int PORT = (u_short) atoi(targets[i].port.c_str());
			targets[i].socketfd = socket(AF_INET, SOCK_STREAM, 0);
			if(targets[i].socketfd < 0){
				continue;
			}
			memset(&(targets[i].sa), sizeof(targets[i].sa), 0);
			targets[i].sa.sin_family = AF_INET;
			targets[i].sa.sin_addr.s_addr = *((unsigned long*)he->h_addr);
			targets[i].sa.sin_port = htons(PORT);
		}	
	}
	
	for(int i=0; i<targets.size(); i++){
		EditPrintf(hwndEdit, TEXT("===files: %s\n"),targets[i].test_file.c_str());
  		if( (targets[i].filefp = fopen(targets[i].test_file.c_str(),"r")) == NULL){
            //perror("open testfile");
            continue;
        }

	}
	return 0;
}

int serve_file(SOCKET client, const char* filename){
	FILE *resource = NULL;
    int numchars = 1;
	char buf[BUFFER_SIZE];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = recv(client, buf, sizeof(buf),0);
    //printf("openning file: [%s]\n",filename);
    resource = fopen(filename, "r");
    if (resource == NULL)
		fprintf(stderr,"ERROR File not found \n", numchars);	
    else
    {
        string buffer("HTTP/1.0 200 OK\r\n");
		int numchars_send = send(client, buffer.c_str(), buffer.length(), 0);
		char buff[BUFFER_SIZE];
		fgets(buff, sizeof(buff), resource);
		while(!feof(resource)){
			send(client,buff,strlen(buff),0);
			fgets(buff,sizeof(buff), resource);
		}
        fclose(resource);
    }

	return 0;
}

void parse_query(char* str, char** h, char** port, char** f){
	char* ptr;
    int i=0;
    int len = strlen(str);
    int top=0;
    char *p, *q;
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
            if(i%3==0)
                h[top] = NULL;
            if(i%3==1)
                port[top] = NULL;
            if(i%3==2)
                f[top++] = NULL;
        }
        else{
            char* t = new char [1024];
            strncpy(t, p, q-p);
			t[q-p]='\0';
            if(i%3==0)
                h[top] = t;
            if(i%3==1)
                port[top] = t;
            if(i%3==2)
                f[top++] = t;
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

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [1024] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
	 return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}

char* decode_html(char* str){
    char* res = new char[BUFFER_SIZE];
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


int get_line(SOCKET sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')&& (c != '\r')){
        n = recv(sock, &c, 1, 0);
		//EditPrintf(hwndEdit, TEXT("=== Bytes received: %d\n CHAR: [%c]\n"),n, c);
        if (n > 0){
            if (c == '\r'){
                n = recv(sock, &c, 1, MSG_PEEK);
				//EditPrintf(hwndEdit, TEXT("===  Bytes received: %d\n CHAR: [%c]\n"),n, c);
                if ((n > 0) && (c == '\n')){
                    recv(sock, &c, 1, 0);
                }
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
		else if(n<0){
			//EditPrintf(hwndEdit, TEXT("===Recv Error %d\n"),WSAGetLastError());
			c = '\n';
		}
        else
            c = '\n';
    }
    buf[i] = '\0';
    return(i);
}
void print_col_html_begin(SOCKET client){
	char buffer[BUFFER_SIZE*3];
    sprintf(buffer,"Content-Type: text/html\n\n");
    sprintf(buffer + strlen(buffer),"<html>\n");
    sprintf(buffer + strlen(buffer),"<head>\n");
    sprintf(buffer + strlen(buffer),"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
    sprintf(buffer + strlen(buffer),"<title>Network Programming Homework 3</title>\n");
    sprintf(buffer + strlen(buffer),"</head>\n");
    sprintf(buffer + strlen(buffer),"<body bgcolor=#336699>\n");
    sprintf(buffer + strlen(buffer),"<font face=\"Courier New\" size=2 color=#FFFF99>\n");
    sprintf(buffer + strlen(buffer),"<table width=\"800\" border=\"1\">\n");
    sprintf(buffer + strlen(buffer),"<tr>\n");
    for(int i = 0; i < targets.size(); i++) 
		sprintf(buffer + strlen(buffer),"<td>%s</td>\n", targets[i].ip.c_str());
    sprintf(buffer + strlen(buffer),"<tr>\n");
    sprintf(buffer + strlen(buffer),"<td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"></td><td valign=\"top\" id=\"m3\"></td><td valign=\"top\" id=\"m4\"></td></tr>\n");
    sprintf(buffer + strlen(buffer),"</table>\n");
	
	send(client, buffer, strlen(buffer),0);
}

void print_col_html_end(SOCKET client){
	char buffer[BUFFER_SIZE];
    sprintf(buffer, "</font>\n");
    sprintf(buffer + strlen(buffer),"</body>\n");
    sprintf(buffer + strlen(buffer),"</html>\n");
	send(client, buffer, strlen(buffer), 0);
}

void send_inject_script(SOCKET browser, int id, char* msg){
	char outbuf[300000];
    if(msg[strlen(msg)-1] == '\n' || msg[strlen(msg)-1] == '\r')
        msg[strlen(msg)-1] = '\0';
    const char* templatee = "<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n";
    const char* template_withoutbr = "<script>document.all['m%d'].innerHTML += \"%s\";</script>\n";
//    int i;
    if(strcmp(msg, "% ")==0)
    {
        sprintf(outbuf, template_withoutbr, id, msg);
    }
    else
    {
        sprintf(outbuf, templatee, id, msg);
    }
	send(browser,outbuf, sizeof(outbuf), 0);
    //printf("%s", outbuf);
    fflush(stdout);
}

void feed_line_to_server(int i){
	char line[BUFFER_SIZE];
    size_t len = 0;
	int count, it;
	memset(line,0,BUFFER_SIZE);
	char c;
	it = 0;
	for(count=1; count<BUFFER_SIZE; count++){
		if(fread(&c,1,1,targets[i].filefp)){
			if((c=='\r') ||(c=='\n' )){
				line[it++]='\0';
				break;
			}else{
				line[it++]=c;
			}
		}
	}
	EditPrintf(hwndEdit, TEXT("===File line: %s\n"),line);
                
	//grepline(targets[i].filefp, line);
    //printf("FEED line: [%s] to [%d]\n", line,targets[i].socketfd);
    if(send(targets[i].socketfd, line, strlen(line), 0) < 0){
        //perror("feed_line write failed");
        return;
    }
	char temp[BUFFER_SIZE]; 
	strcpy(temp,line);
    char* t = strtok(temp,"%\r\n");
    while(t != NULL){
		send_inject_script(bro_sock,i,t);
        t = strtok(NULL,"%\r\n");
    }
}

