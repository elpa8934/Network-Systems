/*
Network Systems - CSCI 4273
PA #4 - Distributed File System
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
#include <sys/types.h>
#include <sys/stat.h>
#include <vector> 

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */
#define TOKLEN	 1024

struct file_chunk{
	char name[MAXLINE];
	char path[MAXLINE];
	char user[MAXLINE];
};

struct auth_user {
	char username[TOKLEN];
	char password[TOKLEN];
};

std::vector<auth_user> users;
std::vector<file_chunk> files_stored;
pthread_mutex_t users_mutex; // Mutex for list of authorized users
pthread_mutex_t files_mutex; // mutex for list of files stored
char dfs[8];

// Declare functions
void *thread(void *vargp);
void process_req(int connfd);
int open_listenfd(int port);
int is_auth(int connfd, char* username, char* password);
void add_file(char*path, char*name, char *user);
int is_file_stored(char *path);
void files_clean();
/*
SIGINT handling
*/
void catch_function(int signo) {
	files_clean();
    exit(0);
}

int main(int argc, char **argv)
{
	// Ctrl+c ==> SIGINT signal
	signal(SIGINT, catch_function);
	
	int *connfdp, port, clientfd;
	socklen_t clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 
    size_t line_length;
    char *line;


    // Check given command line arguments
    if (argc != 3) {
		fprintf(stderr, "Invalid Argument - usage: %s /DFS<num> <port>\n", argv[0]);
		exit(0);
    }
    // Check which for valid /DFS<num> argument
    if ((strcmp(argv[1], "/DFS1")) && (strcmp(argv[1], "/DFS2")) && (strcmp(argv[1], "/DFS3")) && (strcmp(argv[1], "/DFS4"))) {
    	fprintf(stderr, "Invalid DFS Number - usage: %s /DFS<num> <port>\n", argv[0]);
    	return 1;
 	}

 	//initialize mutex
    if (pthread_mutex_init(&users_mutex, NULL) != 0){
      printf("Mutex error\n");
      return -1;
    }
    if (pthread_mutex_init(&files_mutex, NULL) != 0){
      printf("Mutex error\n");
      return -1;
    }

	// parse .conf file
	FILE* config = fopen("dfs.conf", "rb");
	if (config == NULL) {
		printf("Error - unable to open config file\n");
		exit(0);
	}
	while (getline(&line, &line_length, config)) {
		char *username = strtok(line, " ");
		char *password = strtok(NULL, "\n");
		if ((username == NULL) || (password == NULL)) {
			break;
		}
		auth_user newUser;
		bzero(newUser.username, TOKLEN);
		bzero(newUser.password, TOKLEN);
		strcpy(newUser.username, username);
		strcpy(newUser.password, password);
		pthread_mutex_lock(&users_mutex);
		// store in user vector
		users.push_back(newUser);
		pthread_mutex_unlock(&users_mutex);
	}
	fclose(config);

	port = atoi(argv[2]);
	// make dfs directory
	struct stat st = {0};
	strcpy(dfs, ".");
 	strcat(dfs, argv[1]);
   	if (stat(dfs, &st) == -1) {
    	mkdir(dfs, 0777);
	}

    clientfd = open_listenfd(port);
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
    process_req(connfd);
    close(connfd);
    return NULL;
}

/*
 * process_req - read and process incoming HTTP requests
 */
void process_req(int connfd) 
{

    char reqbuf[MAXBUF];
    char req[MAXBUF];
    char buf[MAXBUF];
    char filename[MAXBUF];
    char fileinter[MAXBUF];
    char newdirpath[MAXBUF];
    char filepath[MAXBUF];
    char username[MAXBUF];
    char password[MAXBUF];
    char *chunknum;

    bzero(reqbuf, MAXLINE);
    int n = read(connfd, reqbuf, MAXBUF);
    printf("server received the following request:\n%s\n\n",reqbuf);

    // Parse input
    bzero(req, MAXBUF);
    bzero(filename, MAXBUF);
    bzero(password, MAXBUF);
    bzero(username, MAXBUF);
    sscanf(reqbuf, "%s %s %s %s", req, username, password, filename);
	

    // consruct file path
	bzero(filepath, MAXLINE);
	bzero(fileinter, MAXLINE);
	bzero(newdirpath, MAXLINE);
	sprintf(fileinter, "%s/%s/", dfs, username);
	strcpy(filepath, fileinter);

	// make user directory
	struct stat st = {0};
	strcpy(newdirpath, ".");
 	strcat(newdirpath, filepath);
   	if (stat(filepath, &st) == -1) {
    	mkdir(filepath, 0777);
	}
	strcat(filepath, filename);

	/*
	get - send requested file chunk to client
	*/
    if (!strncmp(reqbuf, "get", 3)) {
    	int rec, n;
    	if (is_auth(connfd, username, password)) {
    		// open and read file
    		FILE* fp = fopen(filepath, "rb");
    		if (fp == NULL) {
    			printf("File failed to open\n");
    		}
    		while (1) {
				bzero(buf, MAXBUF);
				rec = fread(buf, 1, MAXBUF, fp);
		    	if (rec <= 0) {
		    		break;
		    	}
		    	n = write(connfd, buf, rec);
		    	// write to client
		    	if (n < 0) {
					printf("ERROR in write to client");
				}
		    	
		    }
		    fclose(fp);
		    bzero(buf, MAXBUF);
			sprintf(buf, "END");
			// write end message to client
			printf("Sending %s chunk to client...\n", filename);
			sleep(1);
			write(connfd, buf, MAXBUF);
    	}
    }
	/* 
	put - upload specified filechunk onto DFS server
	*/   
	else if (!strncmp(reqbuf, "put", 3)) {
		bzero(buf, MAXBUF);
		// check if authenticated
		if (is_auth(connfd, username, password)) {
			//open file
			FILE *fptr = fopen(filepath, "wb");
			//read in file contents
			while (1) {
                bzero(buf, MAXBUF);
                n = read(connfd, buf, MAXBUF);
                if (!strncmp(buf, "END", 3)) {
                	break;
                }
                fwrite(buf, 1, n, fptr);
            }
            fclose(fptr);
            add_file(filepath, filename, username);
		}
    }
    /* 
	ls - list file chunks available on DFS server
	*/   
	else if (!strncmp(reqbuf, "ls", 2)) {
		// check if authenticated
		if (is_auth(connfd, username, password)) {
			//dfsnum = &dfs[5];
			pthread_mutex_lock(&files_mutex);
			//loop through all cache objects in vector
			for (int i = 0; i < files_stored.size(); i++){
				file_chunk file = files_stored.at(i);
				if (!strcmp(file.user, username)) {
		            bzero(filename, MAXLINE);
		            strcpy(filename, file.name+1);
		            chunknum = &filename[strlen(filename)-1];
		            filename[strlen(filename)-2]='\0';
		            bzero(buf, MAXBUF);
					strcpy(buf, chunknum);
					strcat(buf, filename);
					// send file list
					printf("Sending file name to client...\n");
					sleep(1);
					n = write(connfd, buf, MAXBUF);
					if (n < 0) {
						printf("ERROR in write to client");
					}
				}
			}
			// write END message to client
			pthread_mutex_unlock(&files_mutex);
			bzero(buf, MAXBUF);
			sprintf(buf, "END");
			n = write(connfd, buf, MAXBUF);
			if (n < 0) {
				printf("ERROR in write to client");
			}
		}
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

/*
is_auth checks if username and password are authorized by data in .conf file
*/
int is_auth(int connfd, char* username, char* password){
	int n;
	char buf[MAXBUF];
	pthread_mutex_lock(&users_mutex);
	// store in user vector
	for (int i = 0; i < users.size(); i++){
		auth_user u = users.at(i);
		if (!strcmp(u.username, username) && !strcmp(u.password, password)) {
			pthread_mutex_unlock(&users_mutex);
			bzero(buf, MAXBUF);
			sprintf(buf, "OK");
			n = write(connfd, buf, strlen(buf));
			if (n < 0) {
				printf("ERROR in write to client");
				return 0;
			}
			return 1;
		}
	}
	pthread_mutex_unlock(&users_mutex);
	bzero(buf, MAXBUF);
	sprintf(buf, "Invalid Username/Password. Please try again");
	n = write(connfd, buf, strlen(buf));
	if (n < 0) {
		printf("ERROR in write to client");
		return 0;
	}
	printf("User authentication failed\n");
	return 0;
}

/*
add_file - adds file object to data structure keeping track of available files
*/
void add_file(char*path, char*name, char *user) {
	// check if file already in list
	int alreadyIn = 0;
	pthread_mutex_lock(&files_mutex);
	for (int i = 0; i < files_stored.size(); i++) {
		// check if name matches
		if (!strcmp(files_stored.at(i).name, name)) {
			alreadyIn = 1;
			break;
		}
	}
	if (!alreadyIn) {
		file_chunk newFile;
		strcpy(newFile.path, path);
		strcpy(newFile.name, name);
		strcpy(newFile.user, user);
		files_stored.push_back(newFile);
	}
	pthread_mutex_unlock(&files_mutex);
}

/*
is_file_stored - checks if requested file is stored, returns 0 or 1
*/
int is_file_stored(char *path) {
	pthread_mutex_lock(&files_mutex);
	// loop through all cache objects in vector
	for (int i = 0; i < files_stored.size(); i++){
		file_chunk file = files_stored.at(i);
		// check if name matched and not timed out
		if (!strcmp(file.path, path)) {
			pthread_mutex_unlock(&files_mutex);
			return 1;
		}
	}
	pthread_mutex_unlock(&files_mutex);
	return 0;

}

/* 
Remove DFS folder
*/
void files_clean() {
	char folderCommand[256];
	sprintf(folderCommand, "exec rm -r %s", dfs);
	system(folderCommand);
}