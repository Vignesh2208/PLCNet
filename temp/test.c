#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/poll.h>

int main(int argc, char ** argv)
{
	int pid;
	pid = fork();
	if (pid == 0) {
		 system("python test.py");
	}
	else{
		while(1);
	}
	return 0;
}
	

