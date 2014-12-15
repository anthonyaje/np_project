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
    FILE * fp;

    fp = fopen("main.c", "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    char* str = readline(fp);
    printf("%s", str);

    str = readline(fp);
    printf("%s", str);
    
    fclose(fp);
    exit(EXIT_SUCCESS);
}
