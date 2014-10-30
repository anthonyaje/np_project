#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

int num_of_pipe(char* line){
    int i,n;
    n=0;
    for(i=0; i<strlen(line); i++){
        if(line[i]=='|') n++;
    }

return n+1;
}


int main(){
char token[15]={""};
int i,j,pid;
int pipefd[2];

token[0]='a';
token[1]='j';
token[2]='e';

printf("sizeof token: %d", (strlen(token)));
//pipe(pipefd);

//write(pipefd[1],"hello",5);
/*
pid = fork();
if(pid==0){
	printf("child begin\n");
	int temp_pipe[2];
	pipe(temp_pipe);
	
	write(temp_pipe[1],"world",5);
	
	char buf1[15]={};
	char buf2[15]={};
	read(pipefd[0],buf1,15);
	read(temp_pipe[0],buf2,15);
	printf("buf1: [%s]\n",buf1);
	printf("buf2: [%s]\n",buf2);
	strcat(buf1,buf2);
	write(pipefd[1],buf1,15);
	read(pipefd[0],buf1,15);
	strcat(buf1,buf2);
	write(pipefd[1],buf1,15);
	close(pipefd[0]);
	close(pipefd[1]);
	close(temp_pipe[0]);
	close(temp_pipe[1]);
	printf("child end\n");
}
else{
	waitpid(pid,&i,0);
	char buffer[20]={};
	close(pipefd[1]);
	read(pipefd[0],buffer,20);
//	printf("buf1: [%s]\n",buf1);
//	printf("buf2: [%s]\n",buf2);
	printf("buffer: [%s]\n",buffer);

}
*/

return 0;
}
