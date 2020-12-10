/*
Network Systems - CSCI 4273
PA #2 - HTTP Server
Liz Parker
10/18/2020
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void process_req(int connfd);
void *thread(void *vargp);
//void error(int connfd);

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
		connfdp = malloc(sizeof(int));
		*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* thread routine */
void * thread(void * vargp) 
{  
	// Get the socket descriptor
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    process_req(connfd);
    close(connfd);
    return NULL;
}

/* 
Send a generic error to the client
In this assignment, all error messages can be treated as the “500 Internet Server Error” 
*/
void error(int connfd) 
{
	char err[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type:text/plain\r\nContent-Length:0\r\n\r\n";
	write(connfd, err,strlen(err));
}

/* return content type */
char *get_type(char* fileExtension)
{
    char *type = strrchr(fileExtension,'.');
	if(!strcmp(type, ".html")){
		return "text/html";
	} else if (!strcmp(type, ".txt")){
		return "text/plain";
	} else if (!strcmp(type, ".png")){
		return "image/png";
	} else if (!strcmp(type, ".gif")){
		return "image/gif";
	} else if (!strcmp(type, ".jpg")){
		return "image/jpg";
	} else if (!strcmp(type, ".css")){
		return "text/css";
	} else if (!strcmp(type, ".js")){
		return "application/javascript";
	}
	return "";
}

// (echo -en "GET http://netsys.cs.colorado.edu HTTP/1.0") | nc 127.0.0.1 48888

/*
 * process_req - read and echo text lines until client closes connection
 */
void process_req(int connfd) 
{
    size_t n; 
    char buf[MAXLINE];
    char header[MAXLINE];  

    n = read(connfd, buf, MAXLINE);
    printf("server received the following request:\n%s\n",buf);

    // The request method should be capital letters like “GET”, “HEAD”, and “POST”
    char *method = strtok(buf, " ");
    //The request URL is a set of words which are case insensitive and 
    // separated by “/” and the server should treat the URL as a relative 
    // path of the current document root directory.
    char *uri = strtok(NULL, " "); 
    // The request version follows the rule like “HTTP/x,y” where x and y are numbers.
    char *version = strtok(NULL, "\r\n"); 

    // DEBUG info
    printf("Request type: %s\nFilename: %s\nVersion: %s\n", method, uri, version);

    // Handle null edge cases
    if (method == NULL) {
    	printf("Null method - sending error\n");
    	error(connfd);
    	return;
    } else if (uri == NULL) {
		printf("Invalid file name requestion - sending error\n");
    	error(connfd);
    	return;
    } else if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) {
    	printf("Invalid version - sending error\n");
    	error(connfd);
    	return;
    }

    // check which method requested
    if (!strcmp(method, "GET") || !strcmp(method, "POST")) {
        // Default web page
        if (!strcmp(uri, "/") || !strcmp(uri, "/inside/")) { 
            bzero(uri, MAXLINE);
            sprintf(uri, "index.html"); // Delfaut page
        } else {
            uri = uri + 1; // remove initial slash
        }

        // Open requested file
        FILE *fptr = fopen(uri, "rb");
        if (fptr == NULL) {
            printf("File not found: %s - sending error\n", uri);
            error(connfd);
            return;
        }

        // Find file size
        fseek(fptr, 0, SEEK_END);
        int fsize = ftell(fptr);
        rewind(fptr);
        //printf("File size: %d\n\n", fsize);

        // get string for content type
        char *type = get_type(uri);

        // Put file size into string for Content-Length
        char fsizestr[MAXLINE];
        sprintf(fsizestr, "%d", fsize);

        // Process POST request
        if (!strcmp(method, "POST")) {
            // Parse Post request
            // Skip "Host: "
            char *garb1 = strtok(NULL, " ");
            // Parse host
            char *host = strtok(NULL, "\r");
            // Skip "Connection: "
            char *garb2 = strtok(NULL, " ");
            // Parse connection
            char *connection = strtok(NULL, "\r\n"); 
            // Parse content
            char *POSTcontent = strtok(NULL, "");
            // Remove extra characters
            POSTcontent = POSTcontent + 3;
            // Check for POST content larger than 1 packet
            if (strlen(POSTcontent) > MAXLINE) {
                printf("Post request size, %lu, too large\n", strlen(POSTcontent));
                error(connfd);
                return;
            }
            // Construct the packet header
            bzero(header, MAXLINE);
            strcpy(header, "HTTP/1.1 200 OK\r\nContent-Type: ");
            strcat(header,type);
            strcat(header,"\r\nContent-Size: ");
            // Put file size into string for Content-Length
            int clength = fsize + strlen(POSTcontent);
            char clengthstr[MAXLINE];
            sprintf(clengthstr, "%d", clength);
            strcat(header,clengthstr);
            strcat(header,"\r\n");
            // Send packet header
            write(connfd, header, strlen(header));
            // Construct the HTML with posted content
            char toPost[MAXLINE];
            bzero(toPost, MAXLINE);
            strcpy(toPost, "<html><body><pre><h1>");
            strcat(toPost,POSTcontent);
            strcat(toPost,"</h1></pre>");
            // Send the HTML with posted content
            write(connfd, toPost, strlen(toPost));
            
        // process GET request
        } else {
            // Construct the packet header
            bzero(header, MAXLINE);
            strcpy(header, "HTTP/1.1 200 Document Follows\r\nContent-Type:");
            strcat(header,type);
            strcat(header,"\r\nContent-Length:");
            // Put file size into string for Content-Length
            char fsizestr[MAXLINE];
            sprintf(fsizestr, "%d", fsize);
            strcat(header,fsizestr);
            strcat(header,"\r\n\r\n");
            // Send packet header
            write(connfd, header, strlen(header));
        }
        // Read and write file contents in chunks
        char content[MAXLINE];
        int chunk = 0;
        while (fsize > 0) {
            bzero(content, MAXLINE);
            if (fsize > MAXLINE) {
                chunk = fread(content, 1, MAXLINE, fptr);
            } else {
                chunk = fread(content, 1, fsize, fptr);
            }
            write(connfd, content, chunk);
            fsize = fsize - chunk;
        }
    } else {
     	printf("Invalid HTTP method requested\n");
     	error(connfd);
     	return;
    }   
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

