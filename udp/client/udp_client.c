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

#define BUFSIZE 1024

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
    char uinput[BUFSIZE]; // buffer for user input
    char sreply[BUFSIZE]; // buffer for server reply
    char file[BUFSIZE]; // buffer for filename
    FILE* fileptr = NULL;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
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
    serverlen = sizeof(serveraddr);

    while (1){

        /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("\nPlease type any of the following commands:\nget [file_name]\nput [file_name]\ndelete [file_name]\nls\nexit\n");
        fgets(buf, BUFSIZE, stdin);

        // Parse input
        sscanf(buf, "%s %s", uinput, file);

        // get [file_name] - The server transmits the requested file to the client
        if (!strncmp(uinput, "get", 3)) {
            // Send packet requesting file from server
            printf("Getting %s from server\n", file);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            // Loop waiting for server response
            while (1) {
                bzero(buf, BUFSIZE);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if (n < 0) error("ERROR in recvfrom");
                sscanf(buf, "%s", sreply);
                //  Process server response
                if (!strncmp(sreply, "END", 3)) {
                    // END signals final packet, break out of loop
                    printf("Fully received %s from server\n", file);
                    fclose(fileptr);
                    break;
                }
                else if (!strncmp(sreply, "ERR", 3)) {
                    // Handle file DNE error
                    printf("ERR The requested file, %s, not found by server\n", file);
                    break;
                }
                else {
                    // Write the packet to the file
                    fileptr = fopen(file, "wb");
                    if(fileptr == NULL){
                        error("Error opening file for writing");
                        break;
                    }
                    fwrite(buf, 1, BUFSIZE, fileptr);
                }
            }
        }
        // put [file_name] - The server receives the transmitted file by the client and stores it locally
        else if (!strncmp(uinput, "put", 3)) {
          // check if file exists
          if (access(file, F_OK ) == -1 ) {
            printf("ERR The requested file, %s, not found\n", file);
          }
          else {
            // Send put request to server
            printf("Sending %s to server\n", file);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            bzero(buf, BUFSIZE);
            // send file in BUFSIZE chunks
            FILE* fileptr = fopen(file, "rb");
            while (fread(buf, 1, BUFSIZE, fileptr)) {
              n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
              if (n < 0) error("ERROR in sendto");
              //bzero(buf, BUFSIZE);
            }
            // Send END packet signaling the server can stop listening
            sprintf(buf, "END");
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            // Clean up
            bzero(buf, BUFSIZE);
            fclose(fileptr);
          }
        }
        // delete [file_name] - The server delete file if it exists. Otherwise do nothing
        else if (!strncmp(uinput, "delete", 6)) {
            // Send delete request to server
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            // Loop waiting for server reply
            while (1) {
                bzero(buf, BUFSIZE);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if (n < 0) error("ERROR in recvfrom");
                sscanf(buf, "%s", sreply);
                // END signals final packet, break out of loop
                if (!strncmp(sreply, "END", 3)) {
                    printf("Deleted %s from server\n", file);
                    break;
                }
                // Handle error case of file DNE
                else if (!strncmp(sreply, "ERR", 3)) {
                    printf("ERR The requested file, %s, not found by server\n", file);
                    break;
                }
            }
        }
        // ls - search all the files in its local directory and send a list of all these files to the client
        else if (!strncmp(uinput, "ls", 2)) {
            printf("Getting file list from server...\n");
            // Send ls request to server
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            // Loop waiting for server response
            while (1) {
                bzero(buf, BUFSIZE);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if (n < 0) error("ERROR in recvfrom");
                sscanf(buf, "%s", sreply);
                // END signals final packet, break out of loop
                if (!strncmp(sreply, "END", 3)) {
                    break;
                }
                else {
                    printf("%s\n", buf);
                }
            }
        }
        // exit - The server should exit gracefully
        else if (!strncmp(uinput, "exit", 4)) {
            printf("Sending exit request to server...\n");
            // Send exit request to server
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");
            // Loop waiting for server reply
            while (1) {
                bzero(buf, BUFSIZE);
                n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if (n < 0) error("ERROR in recvfrom");
                printf("%s\n", buf);
                break;
            }
            return 0;
        }
        // all other inputs - y repeat the command back to the client with no modification
        // stating that the given command was not understood
        else {
            printf("%s was not understood", uinput);
        }
    }
}
