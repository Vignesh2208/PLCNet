/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h> 
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <fcntl.h>
#include <sys/poll.h>
#include <time.h>

#define BUFSIZE 1024

void gettimeofdayoriginal(struct timeval *tv, struct timezone *tz) {
#ifdef __x86_64
  syscall(314, tv, tz);
  return;
#endif
  syscall(351, tv, tz);
  return;
}

void print_time(){

  struct timeval later;
  struct timeval later1;
  struct tm localtm;
  struct tm origtm;

  gettimeofday(&later, NULL);
  gettimeofdayoriginal(&later1, NULL);
  localtime_r(&(later.tv_sec), &localtm);
  localtime_r(&(later1.tv_sec),&origtm);
  //printf("%d %d virtual time: %ld:%ld physical time: %ld:%ld localtime: %d:%02d:%02d %ld\n",x,getpid(),later.tv_sec-now.tv_sec,later.tv_usec-now.tv_usec,later1.tv_sec-now1.tv_sec,later1.tv_usec-now1.tv_usec,localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec);

  printf("curr : localtime: %d:%02d:%02d %ld, orig_time : %d:%02d:%02d %ld\n", localtm.tm_hour, localtm.tm_min, localtm.tm_sec, later.tv_usec, origtm.tm_hour, origtm.tm_min, origtm.tm_sec, later1.tv_usec);
  fflush(stdout);
}



/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    
     int numMessages = atoi(argv[3]);
    
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    char *s = inet_ntoa(serveraddr.sin_addr);
    printf("Server IP address : %s\n",s);

    /* get a message from the user */
    
    int cc = 0;
		

	//For debugging SocketHook
    	FILE *fp;
	// Enter your fav path here.
	fp = fopen("/home/vignesh/client.txt","a");
	for (cc = 0; cc < numMessages; cc++)
	{
		bzero(buf, BUFSIZE);
		sprintf(buf, "Ping SEQ %.4d", cc);

		/* send the message to the server */

		// For debugging SocketHook		
		struct timeval sendTimeStamp,JAS_Timestamp;
		gettimeofday(&sendTimeStamp, NULL);
		
		long sendTS = sendTimeStamp.tv_sec * 1000000 + sendTimeStamp.tv_usec;

		serverlen = sizeof(serveraddr);
		n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
		printf("Send Time : \n");
		print_time();
		//For debugging SocketHook		
		gettimeofday(&JAS_Timestamp,NULL);
		fprintf(fp,"Send secs = %d, Send usecs = %d, JAS secs = %d, JAS usecs = %d\n",sendTimeStamp.tv_sec,sendTimeStamp.tv_usec, JAS_Timestamp.tv_sec, JAS_Timestamp.tv_usec);
		
		if (n < 0){ 
			// fOR DEBUGGING SOCKEThOOK
			//fclose(fp);
		  error("ERROR in sendto");
		}

		/* print the server's reply */
		n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);

		struct timeval receiveTimestamp;
		gettimeofday(&receiveTimestamp, NULL);
		long recvTS = receiveTimestamp.tv_sec * 1000000 + receiveTimestamp.tv_usec;
		printf("%ld\n", recvTS - sendTS);

		if (n < 0) {
			//For debugging SocketHook
			fclose(fp);
		  error("ERROR in recvfrom");
		}
		printf("Echo from server: %s\n", buf);
	
	}

	//For debugging SocketHook
	//fclose(fp);
	return 0;
}
