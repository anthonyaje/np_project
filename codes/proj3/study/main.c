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
    const char* t = "i love %s. %d times a day";
    sprintf(buff,t,"deborah",5);
    printf("buff: [%s]\n",buff);    

    exit(EXIT_SUCCESS);
}
