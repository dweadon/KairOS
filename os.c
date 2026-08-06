#include <stdio.h>      // printf, fgets, popen — input/output
#include <stdlib.h>     // malloc, free, exit, getenv
#include <string.h>     // strtok, strcmp, strcpy — string handling
#include <unistd.h>     // fork, execvp, read, write, close
#include <sys/wait.h>   // waitpid — wait for commands to finish
#include <sys/types.h>  // pid_t and other basic types
#include <signal.h>     // signal — control what Ctrl+C does
#include <readline/readline.h> // readline — prompt with line editing
#include <readline/history.h>  // add_history — up/down arrow recall


char *home_dir(void){
    char *home = getenv("HOME");
    return home ? home : "/"; // HOME may be unset on a minimal system
}

void go(char *input){
    char *path;
    if (strcmp(input, "go") == 0) {
        path = home_dir();
    } else {
        path = input + 3;   // everything after first 3 symbols, used in order to not count go.
    }
    if (chdir(path) != 0) {
        perror("go");
    }
}
void executes (char *input){
    pid_t pid = fork();
    if (pid == 0){
        signal(SIGINT, SIG_DFL); // let Ctrl+C kill the running command, not the shell
        execlp("/bin/sh", "sh",  "-c", input, NULL);
        printf("execlp failed\n");
        exit(1);
    }
    else{
        wait(NULL);
    }
}

int main() {
    char cwd[1028];
    char prompt[1100];
    char histfile[1100];
    char *input;
    system("clear");
    signal(SIGINT, SIG_IGN); // Ctrl+C shouldn't kill the shell itself

    snprintf(histfile, sizeof(histfile), "%s/.os_history", home_dir());
    read_history(histfile);
    stifle_history(1000); // cap history so the file doesn't grow forever

    while(1){
        getcwd(cwd, sizeof(cwd));
        snprintf(prompt, sizeof(prompt), "@main~%s$ ", cwd);

    input = readline(prompt);
    if (input == NULL){ // Ctrl+D
            break;
    }
    if (input[0] != '\0'){
        add_history(input);
    }

    if (strcmp(input, "exit") == 0){
            free(input);
            break;
        }
    if (strncmp(input, "go", 2) == 0){
        go(input);
        free(input);
        continue;
    }
    executes(input);
    free(input);

}
    write_history(histfile);
}