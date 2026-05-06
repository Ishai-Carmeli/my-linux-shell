#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments");
        return 1;
    }

    int pipefd[2];
    char buffer[2048];

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }

    if (pid != 0) {
        close(pipefd[0]); 
        write(pipefd[1], argv[1], strlen(argv[1]));
        close(pipefd[1]); 
        waitpid(pid, NULL, 0);
        exit(0); 

    } else {
        close(pipefd[1]); 
        int bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Child received: %s\n", buffer);
        }

        close(pipefd[0]);
        _exit(0); 
    }

    return 0;
}