#include "../include/LineParser.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int debug_mode = 0;

void execute(cmdLine* pCmdLine) {
    char* cmd = pCmdLine->arguments[0];
    if (strcmp(cmd, "cd") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (chdir(pCmdLine->arguments[1]) == -1) {
            perror("cd failed");
        }

        return;
    } else if (strcmp(cmd, "stop") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "stop: missing process id\n");
            return;
        }

        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGTSTP) == -1) {
            perror("stop failed");
        }

        return;
    } else if (strcmp(cmd, "wakeup") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "wakeup: missing process id\n");
            return;
        }

        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGCONT) == -1) {
            perror("wakeup failed");
        }

        return;
    } else if (strcmp(cmd, "ice") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "ice: missing process id\n");
            return;
        }

        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGINT) == -1) {
            perror("ice failed");
        }

        return;
    } else if (strcmp(cmd, "nuke") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "nuke: missing process id\n");
            return;
        }

        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(-targetPid, SIGKILL) == -1) {
            perror("nuke failed");
        }

        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        if (pCmdLine->inputRedirect != NULL) {
            int fd_in = open(pCmdLine->inputRedirect, O_RDONLY);
            if (fd_in == -1) {
                perror("input redirection failed");
                _exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        if (pCmdLine->outputRedirect != NULL) {
            int fd_out = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1) {
                perror("output redirection failed");
                _exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        if (execvp(cmd, pCmdLine->arguments) == -1) {
            perror("Execution failed");
            _exit(1);
        }
    } else {
        if (debug_mode) {
            fprintf(stderr, "PID: %d\n", pid);
            fprintf(stderr, "Executing program: %s\n", cmd);
            if (pCmdLine->blocking) {
                fprintf(stderr, "Mode: Foreground\n");
            } else {
                fprintf(stderr, "Mode: Background\n");
            }
        }

        if (pCmdLine->blocking) {
            waitpid(pid, NULL, 0);
        }
    }
}

int main(int argc, char** argv) {
    char path[PATH_MAX];
    char input[2048];
    cmdLine* pCmdL = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    while (1) {
        if (getcwd(path, PATH_MAX) == NULL) {
            perror("getcwd error");
            exit(1);
        }
        printf("%s: ", path);
        if (fgets(input, 2048, stdin) == NULL) {
            break;
        }

        if (strcmp(input, "\n") == 0) {
            continue;
        }

        pCmdL = parseCmdLines(input);

        if (pCmdL == NULL) {
            continue;
        }

        if (strcmp(pCmdL->arguments[0], "quit") == 0) {
            freeCmdLines(pCmdL);
            break;
        }

        execute(pCmdL);
        freeCmdLines(pCmdL);
    }

	puts("\n");
}