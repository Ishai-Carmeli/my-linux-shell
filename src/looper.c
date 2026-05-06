#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>

void handler(int sig)
{
	printf("\nRecieved Signal : %s\n", strsignal(sig));
	fflush(stdout);
	signal(sig, SIG_DFL);
	raise(sig);
	if (sig == SIGTSTP) {
		signal(SIGCONT, handler);
	} else if (sig == SIGCONT) {
		signal(SIGTSTP, handler);
	}
}

int main(int argc, char **argv)
{

	printf("Starting the program\n");
	fflush(stdout);
	signal(SIGINT, handler);
	signal(SIGTSTP, handler);
	signal(SIGCONT, handler);

	while (1)
	{
		sleep(1);
	}

	return 0;
}