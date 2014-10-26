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
char a[10] = "abc";
char token[15]={};
int i,j;

token[0]='x';
token[1]='y';
token[2]='z';

strcat(token,a);

//puts(token);

for(i=0; i<10; i++){
	printf("%c",token[i]);
}


return 0;
}
