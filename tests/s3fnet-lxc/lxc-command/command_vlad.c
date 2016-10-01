#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <readline/readline.h>
#include "time_functions.h"

#define MAX_BUF 1024

/*
This script will take care of adding the kernel module, adding all LXCs with TDF 0, and stopping the experiment/removing kernel module
Will NOT add simulator LXC or start experiment
*/

int main(int argc, char *argv[])
{
	int numberLXCs;
	char *input;
	int i;
	char mypipe[MAX_BUF];
	char command[MAX_BUF]; //command to write the pipe to
	char whichpipe[MAX_BUF]; //which pipe to write to
	char pidsLXCs[MAX_BUF]; //used for finding mappings between pids and lxcs
	char line[MAX_BUF];
	int err;
	int lxc;
	int fd;
	char *filename;
	int shouldDilate;
	struct timespec tm;
	char lxcName[MAX_BUF];
	tm.tv_sec = 0;
	tm.tv_nsec = 150000000L;
	FILE *fp;
	if (argc < 2) {
		printf("Usage: ./command <filename>\n");
		return 0;
	}
	filename = argv[1];
	printf("filename: %s\n",filename);
	FILE *file;
	
	while(1) {
		input = readline ("Enter a command..\n");
		if (strcmp(input, "exit") == 0) {
			printf("Exiting.. \n");
			break;
		}
		else {
			//extract the int, then the command
			char *ptr = strchr(input, ' ');
			if(ptr) {
   				int index = ptr - input;
				*ptr = '\0';
				ptr = ptr+1;
				printf("target LXC: %s, command is: %s\n", input, ptr);
				/*
				if ( *input == 'a' && *(input+1) == 'l' && *(input+2) == 'l') {
					for (i = 1; i <= numberLXCs; i++) {
						sprintf(command, "%s",ptr);
						sprintf(whichpipe, "/tmp/lxc-%d" , i);
//						printf("WRITE TO PIPE: %s || RUN COMMAND: %s\n", whichpipe, command);
						fd = open(whichpipe, O_WRONLY | O_NONBLOCK );
						write(fd, command, sizeof(command));
						close(fd);
					}
				}
				*/
				//else {
				//lxc = atoi(input);
				sprintf(command, "%s",ptr);
				sprintf(whichpipe, "/tmp/%s" , input);
//				printf("WRITE TO PIPE: %s || RUN COMMAND: %s\n", whichpipe, command);
				fd = open(whichpipe, O_WRONLY | O_NONBLOCK );
				write(fd, command, sizeof(command));
				close(fd);
				//}
			}
			else {
				printf("You wrote: %s, not executing anything\n", input);
			}
		}
	}

	file = fopen(filename, "r");
	while(fgets(line, 80, file) != NULL)
   {
	 /* get a line, up to 80 chars from fr.  done if NULL */
	 sscanf (line, "%s", lxcName);
	 /* convert the string to a long int */
	sprintf(command, "lxc-stop -n %s",lxcName);
	system(command);
	sprintf(command, "lxc-destroy -n %s",lxcName);
	system(command);
	 printf ("%s\n", lxcName);

   }
   fclose(file);


	return 0;
}
