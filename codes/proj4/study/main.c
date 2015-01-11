#include <stdio.h>
#include <stdlib.h>

char* readline(FILE* fp){
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    read = getline(&line, &len, fp);
    return line;
}

int main(void)
{
    char buff[10000];
    
    buff[0] = (unsigned int)255;
    buff[1] = (unsigned int)5;

    printf("%x\n",buff[0]);
    printf("%x\n",buff[1]);

    exit(EXIT_SUCCESS);
}
