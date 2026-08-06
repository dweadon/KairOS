#include <stdio.h>      // printf, fgets, popen — input/output
#include <stdlib.h>     // malloc, free, exit, getenv
#include <string.h>     // strtok, strcmp, strcpy — string handling
#include <unistd.h>     // fork, execvp, read, write, close
#include <sys/wait.h>   // waitpid — wait for commands to finish
#include <sys/types.h>  // pid_t and other basic types


void cd(char *input){
    char *path;
    if (strcmp(input, "cd") == 0) {
        path = getenv("HOME");
    } else {
        path = input + 3;   // everything after first 3 symbols, used in order to not count cd.
    }
    if (chdir(path) != 0) {
        perror("cd");
    }
}
void executes (char *input){
    pid_t pid = fork();
    if (pid == 0){
        execlp("/usr/bin/sh", "sh",  "-c", input, NULL);
        printf("execlp failed\n");
    }
    else{
        wait(NULL);
    }
}

int main() {
    char input[1028];
    char cwd[1028];
    system("clear");
    while(1){
        getcwd(cwd, sizeof(cwd));
        printf("@main~%s$ ", cwd);

    if(fgets(input, sizeof(input), stdin) == NULL){
            break;
    }
    input[strcspn(input, "\n")] = 0;

    if (strcmp(input, "exit") == 0){
            break;
        }
    if (strncmp(input, "cd", 2) == 0){
        cd(input);
        continue;
    }
    executes(input);

}
}