#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]){
	int socketfd, portno, n;
    struct sockaddr_in serv_addr;
	//struct hostent *server;

	char buffer[256];
	char* pname = argv[0];
	
	if(argc<3){
		fprintf(stderr,"usage %s hostname port\n",argv[0]);
		exit(0);
	}
	portno = atoi(argv[2]);
	//create socket
	if( (socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("main: Error opening socket");
		exit(1);
	}
	/*server = gethostbyname(argv[1]);
	if(server == NULL){
		fprintf(stderr,"Error, no such host\n");
		exit(0);
	}
	*/
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr("127.1.1.1");
	/*bcopy((char*)server->h_addr,
			(char*)&serv_addr.sin_addr.s_addr, 
				server->h_length);
	*/
	serv_addr.sin_port = htons(portno);

	//connect to the server
	if(connect(socketfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
		perror("Error connecting");
		exit(1);
	}

	//ask message from user
	printf("enter the message: ");
	bzero(buffer, 256);
	fgets(buffer,255,stdin);
	//send the message to server
	n = write(socketfd, buffer, strlen(buffer));
	if(n<0){
		perror("Error writing to socket");
		exit(1);
	}
	//read the server response
	bzero(buffer,256);
	n = read(socketfd, buffer, 255);
	if(n<0){
		perror("Error reading from socket");
		exit(1);
	}
	printf("server responses: %s\n", buffer);
	
	close(socketfd);
	return 0;
}


