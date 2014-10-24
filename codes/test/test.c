#include <stdio.h>
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
char a[] = "c1 |2 c2 | c3 \r\n";
char* token;
char* pch;
int i,j;
int arr[100]={};

puts(a);

printf("numeber of pipe+1: %d\n",num_of_pipe(a));

char* it=a;
j=0;
for(i=0; i<strlen(a); i++){
	if(*(it+i) == '|'){
		if( *(it+i+1) == ' ')
			arr[j++] = 1;
		else{
			arr[j++] = atoi(it+i+1);
		    *(it+i+1) = ' ';
		}
	}
}

puts(a);

for(i=0;i<10;i++){
    printf("arr: %d\n", arr[i]);
}

pch = strchr(a,'|');
printf("ooo%dooo\n",pch-a+1);

pch = strchr(pch+1,'|');
printf("ooo%dooo\n",pch-a+1);



return 0;
}
