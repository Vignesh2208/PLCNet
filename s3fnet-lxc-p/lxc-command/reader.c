#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/poll.h>
#define MAX_BUF 1024

#include "../base/s3fnet-definitions.h"
// Above #include added by Vlad in order to have a single point (s3fnet-definitions.h) that need to be modified

//This is just a directory to where you want the data to be stored
const char *PATH_TO_DATA = PATH_TO_READER_DATA;

int main()
{
    int fd;
    char buf[MAX_BUF];
    char hostname[MAX_BUF];
    char debug[MAX_BUF];
    char command[MAX_BUF];
    gethostname(hostname,MAX_BUF);
    char myfifo[MAX_BUF];
    sprintf(myfifo, "/tmp/%s", hostname);
    mkfifo(myfifo, 0666);
    /* open, read, and display the message from the FIFO */
    fd = open(myfifo, O_RDWR | O_NONBLOCK );
    struct pollfd ufds;
    int result;
    int rv;
    int pid;
    
    sprintf(debug, "echo Starting Debug for %s > %s/%s", hostname, PATH_TO_DATA, hostname);
    system(debug);
    ufds.fd = fd;
    ufds.events = POLLIN;
while (1) {
    rv = poll(&ufds, 1, -1);
if (rv == -1) {
    perror("poll"); // error occurred in poll()
} else if (rv == 0) {
   printf("rv is 0\n");
 } else {
    // check for events on s1:
    if (ufds.revents & POLLIN) {
        result = read(fd, buf, MAX_BUF); // receive normal data
         if (strcmp(buf, "exit") == 0) {
	    printf("Exiting..\n");
	    break;
    	 }
    pid = fork();
    if (pid == 0) { //in child
    sprintf(debug, "echo Running Command %s >> %s/%s", buf, PATH_TO_DATA, hostname);
    sprintf(command, "%s >> %s/%s 2>&1", buf, PATH_TO_DATA, hostname);
//    sprintf(debug, "echo Running Command %s", buf);
//    sprintf(command, "%s", buf);
    system(debug);
    system(command);
    return 0;
    }
    }
}

}
    close(fd);

    return 0;
}
