#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char **argv)
{
    int i, pid;
	setpgid(0,0);
	
	if(pid=fork()) {
	  i=0;
	}
	else i=1;

	while(1){
	    printf("%d\n", i);
		i+=2;
		sleep(1);
	}
	return 0;
}