#include "../include/LineParser.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct process {
    cmdLine* cmd;
    pid_t pid;
    int status;
    struct process* next;
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

int debug_mode = 0;

process* process_list = NULL;

typedef struct {
    int   number;
    char* line;
} histEntry;

#define HISTLEN 10

histEntry* history_q[HISTLEN] = { NULL };
int hist_head = 0;
int hist_count = 0;
int next_cmd_num = 1;

void applyRedirections(cmdLine* cmd) {
    if (cmd->inputRedirect != NULL) {
        int fd_in = open(cmd->inputRedirect, O_RDONLY);
        if (fd_in == -1) {
            perror("input redirection failed");
            _exit(1);
        }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }

    if (cmd->outputRedirect != NULL) {
        int fd_out = open(cmd->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out == -1) {
            perror("output redirection failed");
            _exit(1);
        }
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }
}

cmdLine* duplicateCmdLine(cmdLine* src) {
    cmdLine* copy = (cmdLine*)calloc(1, sizeof(cmdLine));
    copy->argCount = src->argCount;
    for (int i = 0; i < src->argCount; i++)
        replaceCmdArg(copy, i, src->arguments[i]);
    return copy;
}

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process* node = (process*)malloc(sizeof(process));
    node->cmd    = duplicateCmdLine(cmd);
    node->pid    = pid;
    node->status = RUNNING;
    node->next   = *process_list;
    *process_list = node;
}

void executePipe(cmdLine* left) {
    cmdLine* right = left->next;

    if (left->outputRedirect != NULL) {
        fprintf(stderr, "error: output redirection on the left side of a pipe\n");
        return;
    }

    if (right->inputRedirect != NULL) {
        fprintf(stderr, "error: input redirection on the right side of a pipe\n");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        applyRedirections(left);
        execvp(left->arguments[0], left->arguments);
        perror("execvp");
        _exit(1);
    }

    addProcess(&process_list, left, pid1);
    close(pipefd[1]);

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        close(pipefd[0]);
        waitpid(pid1, NULL, 0);
        return;
    }

    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        applyRedirections(right);
        execvp(right->arguments[0], right->arguments);
        perror("execvp");
        _exit(1);
    }

    addProcess(&process_list, right, pid2);
    close(pipefd[0]);

    if (right->blocking) {
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    }
}

const char* statusName(int status) {
    if (status == RUNNING)   return "Running";
    if (status == SUSPENDED) return "Suspended";
    return "Terminated";
}

void freeProcessList(process* process_list) {
    process* curr = process_list;
    while (curr != NULL) {
        process* next = curr->next;
        freeCmdLines(curr->cmd);
        free(curr);
        curr = next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status) {
    process* curr = process_list;
    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            return;
        }
        curr = curr->next;
    }
}

void updateProcessList(process** process_list) {
    process* curr = *process_list;
    while (curr != NULL) {
        if (curr->status != TERMINATED) {
            int s;
            int ret = waitpid(curr->pid, &s, WNOHANG | WUNTRACED | WCONTINUED);

            if (ret == -1) {
                updateProcessStatus(*process_list, curr->pid, TERMINATED);
            } else if (ret == curr->pid) {
                if (WIFEXITED(s) || WIFSIGNALED(s))
                    updateProcessStatus(*process_list, curr->pid, TERMINATED);
                else if (WIFSTOPPED(s))
                    updateProcessStatus(*process_list, curr->pid, SUSPENDED);
                else if (WIFCONTINUED(s))
                    updateProcessStatus(*process_list, curr->pid, RUNNING);
            }
        }
        curr = curr->next;
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);

    printf("%-8s %-12s %s\n", "PID", "STATUS", "COMMAND");

    process* curr = *process_list;
    while (curr != NULL) {
        printf("%-8d %-12s", curr->pid, statusName(curr->status));
        for (int i = 0; i < curr->cmd->argCount; i++)
            printf(" %s", curr->cmd->arguments[i]);
        printf("\n");
        curr = curr->next;
    }

    process** indirect = process_list;
    while (*indirect != NULL) {
        process* node = *indirect;
        if (node->status == TERMINATED) {
            *indirect = node->next;
            freeCmdLines(node->cmd);
            free(node);
        } else {
            indirect = &node->next;
        }
    }
}

void addToHistory(const char* line) {
    if (hist_count == HISTLEN) {
        free(history_q[hist_head]->line);
        free(history_q[hist_head]);
        hist_head = (hist_head + 1) % HISTLEN;
        hist_count--;
    }

    int back = (hist_head + hist_count) % HISTLEN;

    histEntry* entry = (histEntry*)malloc(sizeof(histEntry));
    entry->number = next_cmd_num;
    next_cmd_num++;

    entry->line = (char*)malloc(strlen(line) + 1);
    strcpy(entry->line, line);

    history_q[back] = entry;
    hist_count++;
}

void freeHistory(void) {
    int i;
    for (i = 0; i < hist_count; i++) {
        int idx = (hist_head + i) % HISTLEN;
        free(history_q[idx]->line);
        free(history_q[idx]);
    }
}

void printHistory(void) {
    int i;
    for (i = 0; i < hist_count; i++) {
        int idx = (hist_head + i) % HISTLEN;
        printf("%d %s", history_q[idx]->number, history_q[idx]->line);
    }
}

const char* lastHistoryLine(void) {
    if (hist_count == 0) {
        return NULL;
    }
    int idx = (hist_head + hist_count - 1) % HISTLEN;
    return history_q[idx]->line;
}

const char* getHistoryLine(int n) {
    int i;
    for (i = 0; i < hist_count; i++) {
        int idx = (hist_head + i) % HISTLEN;
        if (history_q[idx]->number == n) {
            return history_q[idx]->line;
        }
    }
    return NULL;
}

void execute(cmdLine* pCmdLine) {
    char* cmd = pCmdLine->arguments[0];

    if (pCmdLine->next != NULL) {
        executePipe(pCmdLine);
        return;
    }

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
        if (kill(targetPid, SIGSTOP) == -1) {
            perror("stop failed");
        } else {
            updateProcessStatus(process_list, targetPid, SUSPENDED);
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
        } else {
            updateProcessStatus(process_list, targetPid, RUNNING);
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
        } else {
            updateProcessStatus(process_list, targetPid, TERMINATED);
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
    } else if (strcmp(cmd, "procs") == 0) {
        printProcessList(&process_list);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        applyRedirections(pCmdLine);

        if (execvp(cmd, pCmdLine->arguments) == -1) {
            perror("Execution failed");
            _exit(1);
        }
    } else {
        addProcess(&process_list, pCmdLine, pid);

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

        if (strcmp(pCmdL->arguments[0], "history") == 0) {
            printHistory();
            freeCmdLines(pCmdL);
            continue;
        }

        if (pCmdL->arguments[0][0] == '!') {
            const char* resolved = NULL;
            if (strcmp(pCmdL->arguments[0], "!!") == 0) {
                resolved = lastHistoryLine();
            } else {
                int n = atoi(pCmdL->arguments[0] + 1);
                if (n > 0) {
                    resolved = getHistoryLine(n);
                }
            }
            freeCmdLines(pCmdL);

            if (resolved == NULL) {
                printf("error: no such command in history\n");
                continue;
            }

            char saved[2048];
            strcpy(saved, resolved);

            addToHistory(saved);
            cmdLine* pNew = parseCmdLines(saved);
            if (pNew != NULL) {
                execute(pNew);
                freeCmdLines(pNew);
            }
            continue;
        }

        addToHistory(input);
        execute(pCmdL);
        freeCmdLines(pCmdL);
    }

    freeProcessList(process_list);
    freeHistory();
    puts("\n");
}