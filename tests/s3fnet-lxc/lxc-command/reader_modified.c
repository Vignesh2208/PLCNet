#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <sys/epoll.h>
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
    int i = 0;

    struct epoll_event event;
    struct epoll_event *events;
    int efd, n, s;

    efd = epoll_create1 (0);
        
    // allocate events array
    events = calloc(64, sizeof event);
        
    // register read end of pipe using edge-triggered mode with ONESHOT.
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;


    printf("Starting debug\n");
    sprintf(debug, "echo Starting Debug for %s > %s/%s", hostname, PATH_TO_DATA, hostname);
    system(debug);

    // wait on read-end becoming readable.
    //printf("PARENT: waiting on pipe\n");
    //sprintf(debug, "echo waiting on pipe %s >> %s/%s", hostname, PATH_TO_DATA, hostname);
    //system(debug);

    
    
    //ufds.fd = fd;
    //ufds.events = POLLIN;
    while (1) {
        s = epoll_ctl (efd, EPOLL_CTL_ADD, fd, &event);
        rv = epoll_wait (efd, events, 64, -1);
        //rv = poll(&ufds, 1, -1);
        if (rv == -1) {
            perror("epoll error:"); // error occurred in poll()
        } else if (rv == 0) {
            printf("rv is 0\n");
        } else {
        // check for events on s1:
            //if (ufds.revents & POLLIN) {
            result = read(fd, buf, MAX_BUF); // receive normal data
            if (strcmp(buf, "exit") == 0) {
    	        printf("Exiting..\n");
    	        break;
            }
            pid = fork();
            if (pid == 0) { //in child
                printf("Running Command %s\n", buf);
                for(i = 0; i < MAX_BUF; i++)
                    debug[i] = '\0';

                sprintf(debug, "echo Running Command %s >> %s/%s", buf, PATH_TO_DATA, hostname);
                system(debug);

                sprintf(command, "%s >> %s/%s 2>&1", buf, PATH_TO_DATA, hostname);                          
                system(command);
                return 0;
            }
            //}
        }

    }
    close(fd);

    return 0;
}
