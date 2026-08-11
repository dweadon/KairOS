#include <stdio.h>      // printf, fgets, popen — input/output
#include <stdlib.h>     // malloc, free, exit, getenv
#include <string.h>     // strtok, strcmp, strcpy — string handling
#include <unistd.h>     // fork, execvp, read, write, close
#include <sys/wait.h>   // waitpid — wait for commands to finish
#include <sys/types.h>  // pid_t and other basic types
#include <signal.h>     // signal — control what Ctrl+C does
#include <readline/readline.h> // readline — prompt with line editing
#include <readline/history.h>  // add_history — up/down arrow recall
#include <termios.h>    // raw terminal mode for arrow-key navigation
#include <dirent.h>     // opendir/readdir — directory listing
#include <sys/stat.h>   // stat — tell files from directories


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
static void json_escape(const char *in, char *out, size_t outsz){
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < outsz; i++){
        unsigned char c = in[i];
        if (c == '"' || c == '\\'){
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n'){
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (c >= 0x20){
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

#define AI_MAX_TURNS 20   // sliding window of user+assistant messages kept in memory
#define AI_MSG_MAXLEN 2048

static char ai_history_role[AI_MAX_TURNS][10];
static char ai_history_content[AI_MAX_TURNS][AI_MSG_MAXLEN];
static int ai_history_count = 0;

static void ai_history_add(const char *role, const char *content){
    if (ai_history_count >= AI_MAX_TURNS){
        for (int i = 1; i < AI_MAX_TURNS; i++){
            strcpy(ai_history_role[i - 1], ai_history_role[i]);
            strcpy(ai_history_content[i - 1], ai_history_content[i]);
        }
        ai_history_count = AI_MAX_TURNS - 1;
    }
    strncpy(ai_history_role[ai_history_count], role, sizeof(ai_history_role[0]) - 1);
    ai_history_role[ai_history_count][sizeof(ai_history_role[0]) - 1] = '\0';
    strncpy(ai_history_content[ai_history_count], content, AI_MSG_MAXLEN - 1);
    ai_history_content[ai_history_count][AI_MSG_MAXLEN - 1] = '\0';
    ai_history_count++;
}

void ai(char *input){
    char *prompt = input + 2; // skip "ai"
    while (*prompt == ' ') prompt++;
    if (*prompt == '\0'){
        printf("usage: ai <message>\n");
        return;
    }

    ai_history_add("user", prompt);

    FILE *req = fopen("/tmp/ai_req.json", "w");
    if (!req){ perror("ai"); return; }
    fprintf(req, "{\"messages\":[");
    for (int i = 0; i < ai_history_count; i++){
        char esc[AI_MSG_MAXLEN];
        json_escape(ai_history_content[i], esc, sizeof(esc));
        fprintf(req, "%s{\"role\":\"%s\",\"content\":\"%s\"}", i ? "," : "", ai_history_role[i], esc);
    }
    fprintf(req, "]}");
    fclose(req);

    // Goes through a Cloudflare Worker proxy that holds the real Groq key
    // server-side and rate-limits by IP, so no key ever ships in the OS.
    system("curl -s https://kairos-ai-proxy.kairos-ai-proxy.workers.dev "
        "-H \"content-type: application/json\" "
        "-d @/tmp/ai_req.json -o /tmp/ai_resp.json");

    FILE *resp = fopen("/tmp/ai_resp.json", "r");
    if (!resp){ printf("ai: request failed\n"); return; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, resp);
    buf[n] = '\0';
    fclose(resp);

    char *marker = strstr(buf, "\"content\":\"");
    if (!marker){
        printf("ai: %s\n", buf); // show the raw response (likely an error) if parsing fails
        return;
    }
    marker += 11;
    char *end = marker;
    while (*end && !(*end == '"' && *(end - 1) != '\\')) end++;

    char reply[AI_MSG_MAXLEN];
    size_t ri = 0;
    for (char *p = marker; p < end; p++){
        if (*p == '\\' && p + 1 < end){
            p++;
            char c = (*p == 'n') ? '\n' : *p;
            putchar(c);
            if (ri + 1 < sizeof(reply)) reply[ri++] = c;
        } else {
            putchar(*p);
            if (ri + 1 < sizeof(reply)) reply[ri++] = *p;
        }
    }
    putchar('\n');
    reply[ri] = '\0';

    ai_history_add("assistant", reply);
}

#define NAV_MAX_ENTRIES 512
#define NAV_NAME_LEN 256

static int nav_cmp(const void *a, const void *b){
    return strcmp((const char *)a, (const char *)b);
}

void nav(void){
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    static char names[NAV_MAX_ENTRIES][NAV_NAME_LEN];
    int count = 0;
    int sel = 0;

    while (1){
        count = 0;
        DIR *d = opendir(".");
        if (d){
            struct dirent *de;
            while ((de = readdir(d)) != NULL && count < NAV_MAX_ENTRIES){
                if (strcmp(de->d_name, ".") == 0) continue;
                strncpy(names[count], de->d_name, NAV_NAME_LEN - 1);
                names[count][NAV_NAME_LEN - 1] = '\0';
                count++;
            }
            closedir(d);
        }
        qsort(names, count, NAV_NAME_LEN, nav_cmp);
        if (sel >= count) sel = count > 0 ? count - 1 : 0;
        if (sel < 0) sel = 0;

        printf("\x1b[2J\x1b[H");
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("nav: %s  (arrows to move, enter to open, q to quit)\r\n\r\n", cwd);
        for (int i = 0; i < count; i++){
            struct stat st;
            int isdir = (stat(names[i], &st) == 0) && S_ISDIR(st.st_mode);
            if (i == sel) printf("\x1b[7m");
            printf("%s%s", names[i], isdir ? "/" : "");
            if (i == sel) printf("\x1b[0m");
            printf("\r\n");
        }
        fflush(stdout);

        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        if (c == 'q'){
            break;
        } else if (c == '\r' || c == '\n'){
            if (count > 0){
                struct stat st;
                if (stat(names[sel], &st) == 0 && S_ISDIR(st.st_mode)){
                    chdir(names[sel]);
                    sel = 0;
                }
            }
        } else if (c == 0x1b){
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            if (seq[0] == '['){
                if (seq[1] == 'A'){ if (sel > 0) sel--; }
                else if (seq[1] == 'B'){ if (sel < count - 1) sel++; }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

void gui(void){
    mkdir("/tmp/xdg-runtime", 0700); // ok if it already exists
    setenv("XDG_RUNTIME_DIR", "/tmp/xdg-runtime", 1); // inherited by system()/execlp below
    setenv("SHELL", "/usr/bin/os", 1); // weston-terminal launches $SHELL; default /bin/sh has no ai/nav/go

    printf("starting GUI (weston)...\r\n");
    fflush(stdout);
    // WAYLAND_DISPLAY must stay unset here: if weston sees it already set, it
    // assumes it should run nested inside another compositor instead of on DRM.
    system("weston --socket=wayland-0 --idle-time=0 >/tmp/weston.log 2>&1 &");
    sleep(2); // give the compositor time to create its Wayland socket before the terminal connects
    setenv("WAYLAND_DISPLAY", "wayland-0", 1); // now safe: only affects weston-terminal below
    system("weston-terminal &");
}

#define MENU_ITEM_COUNT 5
static const char *menu_items[MENU_ITEM_COUNT] = {"Shell", "GUI", "Filesystem", "Network", "About"};

// shows on boot; returns once "Shell" is selected so main() can enter the prompt loop
static void boot_menu(void){
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    int sel = 0;
    while (1){
        printf("\x1b[2J\x1b[H");
        printf("KairOS\r\n\r\n");
        for (int i = 0; i < MENU_ITEM_COUNT; i++){
            if (i == sel) printf("\x1b[7m");
            printf("%s", menu_items[i]);
            if (i == sel) printf("\x1b[0m");
            printf("\r\n");
        }
        printf("\r\n(arrows to move, enter to select)\r\n");
        fflush(stdout);

        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        if (c == '\r' || c == '\n'){
            if (sel == 0) break; // enter the shell

            printf("\x1b[2J\x1b[H");
            if (sel == 1){
                gui();
                printf("GUI launched in the background (needs a real display, not a serial console).\r\n");
            } else if (sel == 2){
                printf("-- Filesystem --\r\n\r\n");
                fflush(stdout);
                system("df -h");
            } else if (sel == 3){
                printf("-- Network --\r\n\r\n");
                fflush(stdout);
                system("ip a");
            } else if (sel == 4){
                printf("-- About --\r\n\r\n");
                fflush(stdout);
                system("uname -a");
                printf("KairOS -- a minimal shell-based OS\r\n");
            }
            printf("\r\npress any key to return to menu\r\n");
            fflush(stdout);
            char tmp;
            read(STDIN_FILENO, &tmp, 1);
        } else if (c == 0x1b){
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            if (seq[0] == '['){
                if (seq[1] == 'A'){ if (sel > 0) sel--; }
                else if (seq[1] == 'B'){ if (sel < MENU_ITEM_COUNT - 1) sel++; }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
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
    gui(); // launch the graphical desktop automatically on every boot
    boot_menu();

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
    if (strncmp(input, "ai", 2) == 0){
        ai(input);
        free(input);
        continue;
    }
    if (strcmp(input, "nav") == 0){
        nav();
        free(input);
        continue;
    }
    if (strcmp(input, "menu") == 0){
        boot_menu();
        free(input);
        continue;
    }
    if (strcmp(input, "gui") == 0){
        gui();
        free(input);
        continue;
    }
    executes(input);
    free(input);

}
    write_history(histfile);
}