#include <stdio.h>      // printf, fgets, popen — input/output
#include <stdlib.h>     // malloc, free, exit, getenv
#include <string.h>     // strtok, strcmp, strcpy — string handling
#include <unistd.h>     // fork, execvp, read, write, close
#include <sys/wait.h>   // waitpid — wait for commands to finish
#include <sys/types.h>  // pid_t and other basic types

void executes (char *input){
    pid_t pid = fork();
    if (pid == 0){
        execlp("/bin/sh", "sh", "-c", input, NULL);
        printf("execlp failed\n");
    }
    else{
        wait(NULL);
    }
}

int main() {
    char input[1028];
    while(1){
    printf("A\\>> ");
    fgets(input, sizeof(input), stdin);
    executes(input);
}
}