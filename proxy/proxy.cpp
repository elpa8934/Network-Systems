/*
Network Systems - CSCI 4273
PA #3 - HTTP Proxy
Liz Parker
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
#include <time.h>
#include <functional>
#include <vector> 
#include <signal.h>
#include <typeinfo> // for debugging
  
#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

// Define Cache struct
// Has the : Page data, url, ip, page size, 
// last time it got called, and pointer to next page in the cache
struct cache{
	time_t lastCall;
	char loc[MAXLINE];
	char hostname[MAXLINE];
};

// struct for caching DNS services
struct dns {
	struct hostent *ip;
	char hostname[MAXLINE];
};

std::vector<cache> filelist;
std::vector<dns> dnslist;
pthread_mutex_t dns_mutex; // Mutex for hostname cache
pthread_mutex_t cache_mutex; // Mutex fr page cache
int *connfdp;
int *serverfdp;
int timeout;

// Declare functions
int open_clientfd(int port);
void *thread(void *vargp);
void proxy(int connfd);
int connect_to_server(char* hostname, int connfd);
struct hostent* dns_in_cache(char* hostname, int connfd);
int is_blacklisted(dns requested);
void cache_store(char* filename, char *hostname);
int in_cache(char* filename, char* hostname);
void cache_clean();
void extra_clean();
void convert_path(char *path);
void server_error(int connfd);
void blacklist_error(int connfd);
void request_error(int connfd);
void not_found_error(int connfd);
/*
SIGINT handling
*/
void catch_function(int signo) {
    //close(clientfd);
    //close(serverfd);
    extra_clean();
    exit(0);
}

int main(int argc, char **argv)
{
	// Ctrl+c ==> SIGINT signal
	signal(SIGINT, catch_function);
	
	int clientfd, serverfd, port;
	socklen_t clientlen=sizeof(struct sockaddr);
    struct sockaddr clientaddr;
    pthread_t tid; 

    // parse and assign command line argumends
    if (argc < 2 || argc >3) {
		fprintf(stderr, "usage: %s <port> [timeout optional]\n", argv[0]);
		exit(0);
    }
    if (argc == 2) {
    	timeout = 60;
    } else {
    	timeout = atoi(argv[2]);
    }
    port = atoi(argv[1]);

    // open listening client socket
    clientfd = open_clientfd(port);

    //initialize mutex
    if (pthread_mutex_init(&cache_mutex, NULL) != 0){
      printf("Mutex error\n");
      return -1;
    }
    if (pthread_mutex_init(&dns_mutex, NULL) != 0){
      printf("Mutex error\n");
      return -1;
    }

    while (1) {
		connfdp = static_cast<int*>(malloc(sizeof(int)));
		*connfdp = accept(clientfd, (struct sockaddr*)&clientaddr, &clientlen);
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
    // call proxy routine for this thread
    proxy(connfd);
    close(connfd);
    return NULL;
}

/*
 * proxy - read, process, forward, and write HTTP requests
 */
void proxy(int connfd) 
{
    size_t n; 
    char reqbuf[MAXBUF];
    char buf[MAXBUF];
    char file[MAXBUF];
    char *hostname, *uribuf, *checkSlash; 
    int rec;

    bzero(reqbuf, MAXLINE);
    n = read(connfd, reqbuf, MAXBUF);
    printf("server received the following request:\n%s\n",reqbuf);
    // The request method should be capital letters like “GET”, “HEAD”, and “POST”
    char *method = strtok(reqbuf, " ");
    // Skip "http://"
   	// parse URL
    char *url = strtok(NULL, " ") + 7; 
    //printf("url len: %s\n", strlen(url));
    // The request version follows the rule like “HTTP/x,y” where x and y are numbers.
    char *version = strtok(NULL, "\r\n"); 
    //hack to remove extra slash if necessary
    if (url[strlen(url)-1]=='/'){
    	url[strlen(url)-1]='\0'; //null terminater
    }

    char uri[MAXLINE];
    bzero(uri, MAXLINE);
   	strcpy(uri, "/");

    // parse out host name if needed
   	checkSlash = strchr(url, '/');
    if (checkSlash != NULL) {
    	hostname = strtok(url, "/");
    	uribuf = strtok(NULL, " ");
    	strcat(uri, uribuf);
    } else {
    	hostname = url;
    } 

    // DEBUG info
    printf("Request type: %s\nHostname: %s\nuri: %s\nVersion: %s\n", method, hostname, uri, version);
    // Handle null edge cases
    if (method == NULL) {
    	printf("Null method - sending error\n");
    	request_error(connfd);
    } else if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) {
    	printf("Invalid version - sending error\n");
    	request_error(connfd);
    } else if (strcmp(method, "GET")) {
     	printf("Invalid HTTP method requested\n");
     	request_error(connfd);
    } 

    // construct filename without /s
    char uricopy[MAXLINE];
    bzero(uricopy, MAXLINE);
   	strcpy(uricopy, uri);
	bzero(file, MAXLINE);
	strcpy(file, "cache/");
	convert_path(uricopy);
	strcat(file, uricopy);

    // check if file in cache
    if (in_cache(file, hostname)) {
    	printf("File received from cache\n");
    	// open file
    	FILE *fptr = fopen(file, "r+");
    	// read from cache
    	if (fptr!= NULL) {
    		bzero(buf, MAXBUF);
    		while (1) {
    		 	rec = fread(buf, 1, MAXLINE, fptr);
		    	if (rec <= 0) {
		    		break;
		    	}
		    	// write to client
		    	write(connfd, buf, rec);
		    	bzero(buf, MAXBUF);
    		}
    		fclose(fptr);
    	}

    	cache_clean();

    }
    else {
	    // connect proxy to the server
	    int serverfd = connect_to_server(hostname, connfd);
	    if (!(serverfd < 0)) {
		    // Build request for server
		    bzero(buf, MAXLINE);
		    sprintf(buf, "GET %s HTTP/1.0\r\n\r\n", uri);
		    write(serverfd, buf, strlen(buf));
		    printf("\n\nfor sever : %s\n", buf);


		    // open file to store in cache
		    FILE *fptr = fopen(file, "w+");
		    while (1) {
		    	bzero(buf, MAXBUF);
		    	// read from server
		    	rec = read(serverfd, buf, MAXBUF);
		    	if (rec <= 0) {
		    		break;
		    	}
		    	// send to client
		    	write(connfd, buf, rec);
		    	// write into cache
		    	fwrite(buf, 1, rec, fptr);
		    }
		    // clean up
		    fclose(fptr);
			close(serverfd);
		    cache_store(file, hostname);
		    cache_clean();
		}
	}
 }


/*
Connect the proxy to the server requested and return socket
*/
int connect_to_server(char* hostname, int connfd){
	int serversk, optval=1;
	struct sockaddr_in serveraddr;

	// Find the server address associated with the hostname
	/* gethostbyname: get the server's DNS entry */
    struct hostent *server = dns_in_cache(hostname, connfd);

    if (server != NULL) {

	    /* Create a socket descriptor */
	    if ((serversk = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	        return -1;

	    /* Eliminates "Address already in use" error from bind. */
	    if (setsockopt(serversk, SOL_SOCKET, SO_REUSEADDR, 
	                   (const void *)&optval , sizeof(int)) < 0)
	        return -1;

	 	/* build the server's Internet address */
	    bzero((char *) &serveraddr, sizeof(serveraddr));
	    serveraddr.sin_family = AF_INET;
	    bcopy((char *)server->h_addr, 
		  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	    serveraddr.sin_port = htons(80);
	    int serverlen = sizeof(serveraddr);

	    // Connect to the server
	    if (connect(serversk, (struct sockaddr*)&serveraddr, serverlen)<0){
	    	return -1;
	    }
	    return serversk;
	}
	return -1;
}



/* 
 * open_clientfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_clientfd(int port) 
{
    int fd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* fd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(fd, LISTENQ) < 0)
        return -1;
    return fd;
} 

/*/////////////////////////////////////////////////////////////////////////////////////////
Domain name cache and blacklist functions
/////////////////////////////////////////////////////////////////////////////////////////*/
struct hostent* dns_in_cache(char* hostname, int connfd) {
	pthread_mutex_lock(&dns_mutex);
	for (int i = 0; i < dnslist.size(); i++){
		if (!strcmp(dnslist.at(i).hostname, hostname)) {
			printf("DNS retrieved from cache\n");
			pthread_mutex_unlock(&dns_mutex);
			return dnslist.at(i).ip;
		}
	}
	struct hostent* serverIP = gethostbyname(hostname); 
	if (serverIP == NULL) {
		not_found_error(connfd);
		pthread_mutex_unlock(&dns_mutex);
		return NULL;
	} else {
		dns newDNS;
		bzero(newDNS.hostname, MAXLINE);
		strcpy(newDNS.hostname, hostname);
		newDNS.ip = serverIP;
		if (is_blacklisted(newDNS)) {
			blacklist_error(connfd);
			pthread_mutex_unlock(&dns_mutex);
			return NULL;
		} 
		dnslist.push_back(newDNS);
		pthread_mutex_unlock(&dns_mutex);
		return newDNS.ip;
	}
}

int is_blacklisted(dns requested) {
	char entry[256];
	FILE *bl = fopen("blacklist", "r");
	if (bl == NULL) {
		printf("No blacklist found\n");
		return 0;
	}
	char *ipaddr = inet_ntoa(*(struct in_addr *)requested.ip->h_name);
	while (fgets(entry, sizeof(entry), bl)) {
		// remove endline character
		entry[strlen(entry)-1]='\0'; //null terminater 
    	if (!strcmp(entry, requested.hostname) || !strcmp(entry, ipaddr)) {
    		printf("Client requested blacklisted hostname/IP\n");
    		return 1;
    	}
    }
	fclose(bl);
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


/*/////////////////////////////////////////////////////////////////////////////////////////
Cache data structure functions
/////////////////////////////////////////////////////////////////////////////////////////*/
/*
store file in cache
*/
void cache_store(char* filename, char* hostname){
	// create new cache object
	cache newCache;
	bzero(newCache.loc, MAXLINE);
	strcpy(newCache.loc, filename);
	strcpy(newCache.hostname, hostname);
	newCache.lastCall = time(NULL);
	pthread_mutex_lock(&cache_mutex);
	// store in cache vector
	filelist.push_back(newCache);
	pthread_mutex_unlock(&cache_mutex);
}

/*
check if file in cache
*/
int in_cache(char* filename, char* hostname){
	pthread_mutex_lock(&cache_mutex);
	// loop through all cache objects in vector
	for (int i = 0; i < filelist.size(); i++){
		cache obj = filelist.at(i);
		// check if name matched and not timed out
		double timedif = difftime(time(NULL),obj.lastCall);
		if (!strcmp(obj.loc, filename) && !strcmp(obj.hostname, hostname) && (timedif<timeout)) {
			obj.lastCall = time(NULL);
			pthread_mutex_unlock(&cache_mutex);
			printf("File retrieved from cache\n");
			return 1;
		}
	}
	pthread_mutex_unlock(&cache_mutex);
	return 0;
}

/*
clean up cache, remove reference and delete files in timed out
*/
void cache_clean(){
	pthread_mutex_lock(&cache_mutex);
	// loop through all cache objects in vector
	for (int i = 0; i < filelist.size(); i++){
		// remove timed out files and their cache object
		cache obj = filelist.at(i);
		double timedif = difftime(time(NULL),obj.lastCall);
		if (timedif>timeout){
			remove(obj.loc);
			filelist.erase(filelist.begin() + i);
		}
	}
	pthread_mutex_unlock(&cache_mutex);
}

// brute force to make sure no files left on server in case of SIGINT
void extra_clean() {
	system("exec rm -r cache/*");
}	


/*
Coverts file path with slashes to permissible filename format for identification of cached files
ex) /images/example.gif -> -images-example.gif
*/
void convert_path(char *path) {
	for (int i = 0; i < strlen(path); i++){
		if (path[i] == '/') {
			path[i] = '-';
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


/*/////////////////////////////////////////////////////////////////////////////////////////
Error Functions
/////////////////////////////////////////////////////////////////////////////////////////*/
void server_error(int connfd) 
{
	char err[MAXLINE];
	bzero(err, MAXLINE);
   	strcpy(err, "HTTP 500 Internal Server Error\r\n\r\n");
	write(connfd, err,strlen(err));
}

void blacklist_error(int connfd) 
{
	char err[MAXLINE];
	bzero(err, MAXLINE);
   	strcpy(err, "HTTP 403 Forbidden\r\n\r\n");
	write(connfd, err,strlen(err));
}

void request_error(int connfd) 
{
	char err[MAXLINE];
	bzero(err, MAXLINE);
   	strcpy(err, "HTTP 400 Bad Request\r\n\r\n");
	write(connfd, err,strlen(err));
}

void not_found_error(int connfd) 
{
	char err[MAXLINE];
	bzero(err, MAXLINE);
   	strcpy(err, "HTTP 404 Not Found\r\n\r\n");
	write(connfd, err,strlen(err));
}
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////