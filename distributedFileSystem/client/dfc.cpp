/*
Network Systems - CSCI 4273
PA #4 - Disributed File System
Liz Parker
*/

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector> 
#include <openssl/md5.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define MAXIP	 16    /*max characters for IP address */
#define LISTENQ  1024  /* second argument to listen() */

struct file_info{
	char filename[MAXLINE];
	int chunks[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}; // row is chunk, col is server
	int incomplete = 1;

};

struct server_info {
	char ip1[MAXIP];
	char ip2[MAXIP];
	char ip3[MAXIP];
	char ip4[MAXIP];
	int port1;
	int port2;
	int port3;
	int port4;
	char username[MAXBUF];
	char password[MAXBUF];
};

// global data scructure for file info
std::vector<file_info> files;

// Declare functions
void update_file_info(server_info server);
int connect_to_server(char* hostname, int portno);
int send_chunk(int servernum, FILE *fp, server_info server, char *chunkname, int chunkoffset, int chunksize);
void put_on_server(int offset, int chunksize, FILE *fp, int socket, int servernum);
int is_auth(int socket);
int hash_mod(char *fname);

/*
SIGINT handling
*/
void catch_function(int signo) {
	printf("\nGood bye\n");
    exit(0);
}

int main(int argc, char **argv) {
	int socks[4];
	char ip[MAXIP];
	int chunkoffset[4];
	char line[256];
	int socket, portno, n;
	int linenum = 1;
	char *garbage; // vars for parsing 
	char buf[MAXBUF];
	char uinput[MAXBUF]; // buffer for user input
	char file[MAXBUF]; // buffer for filename
	char chunkname[MAXBUF], chunk1name[MAXBUF], chunk2name[MAXBUF], chunk3name[MAXBUF], chunk4name[MAXBUF];

	// Ctrl+c ==> SIGINT signal
	signal(SIGINT, catch_function);

	// Check given commane line arguments
	if (argc != 2) {
		fprintf(stderr, "usage: %s <config-filename.conf>\n", argv[0]);
		exit(0);
    }
	// parse .conf file and set up connections to servers
	FILE* config = fopen(argv[1], "rb");
	if (config == NULL) {
		printf("Error - unable to open config file\n");
		exit(0);
	}
	server_info server;
	while (fgets(line, sizeof(line), config) && (linenum<=6)) {
		if (linenum == 1) { // DFS1
			garbage = strtok(line, " ");
			garbage = strtok(NULL, " ");
			garbage = strtok(NULL, ":");
			strcpy(server.ip1, garbage);
			server.port1 = atoi(strtok(NULL, "\n"));
			linenum++;
		} else if (linenum == 2) { // DFS2
			garbage = strtok(line, " ");
			garbage = strtok(NULL, " ");
			garbage = strtok(NULL, ":");
			strcpy(server.ip2, garbage);
			server.port2 = atoi(strtok(NULL, "\n"));
			linenum++;
		} else if (linenum == 3) { // DFS3
			garbage = strtok(line, " ");
			garbage = strtok(NULL, " ");
			garbage = strtok(NULL, ":");
			strcpy(server.ip3, garbage);
			server.port3 = atoi(strtok(NULL, "\n"));
			linenum++;
		} else if (linenum == 4) { // DFS4
			garbage = strtok(line, " ");
			garbage = strtok(NULL, " ");
			garbage = strtok(NULL, ":");
			strcpy(server.ip4, garbage);
			server.port4 = atoi(strtok(NULL, "\n"));
			linenum++;
		} else if (linenum == 5) { // username
			garbage = strtok(line, " ");
			garbage = strtok(NULL, "\n");
			strcpy(server.username, garbage);
			linenum++;
		} else if (linenum == 6) { //password
			garbage = strtok(line, " ");
			garbage = strtok(NULL, "\n");
			strcpy(server.password, garbage);
			linenum++;
		}
	}
	fclose(config);

	// loop to serve client
	while (1) {
        /* get a message from the user */
        bzero(buf, MAXBUF);
        fflush(stdin);
        printf("\nPlease type any of the following commands:\nget [file_name]\nput [file_name]\nls\n>");
        fgets(buf, MAXBUF, stdin);

        // Parse input
        bzero(uinput, MAXBUF);
        bzero(file, MAXBUF);
        sscanf(buf, "%s %s", uinput, file);

		/* 
		get - downloads all available pieces of a file from all available DFS servers
		*/   
		if (!strncmp(uinput, "get", 3)) {
			// update the file list
			update_file_info(server);
			file_info requested;
			int foundRequest = 0;

			for (int j = 0; j < files.size(); j++) {
                if (!strcmp(files.at(j).filename, file)) {
                	requested = files.at(j);
                	foundRequest = 1;
                	break;
                }
            } if (foundRequest) {
				int incomplete = 0;
				for (int r = 0; r < 4; r++) {
	            	if ((requested.chunks[r][0] == 0) && (requested.chunks[r][1] == 0) && (requested.chunks[r][2] == 0) && (requested.chunks[r][3] == 0)) {
	        			incomplete = 1;
	        			printf("File is incomplete.\n");	
	        			break;
	        		}
	            }
	            if (!incomplete) {
	            	printf("Receiving file...\n");
	            	FILE *fptr = fopen(requested.filename, "wb");
	            	for (int pt = 0; pt < 4; pt++) {
	            		int serverToReq;
	            		for (int ser = 0; ser < 4; ser++) { 
	            			if (requested.chunks[pt][ser] == 1) {
	            				serverToReq = ser;
	            				break;
	            			}
	            		}
	            		// create chunk filenames, find server's IP and port
	            		if (pt == 0) {
	            			bzero(chunkname, MAXBUF);
		    				sprintf(chunkname, ".%s.1", file);
	            		} else if (pt == 1) {
	            			bzero(chunkname, MAXBUF);
		    				sprintf(chunkname, ".%s.2", file);
	            		} else if (pt == 2) {
	            			bzero(chunkname, MAXBUF);
		    				sprintf(chunkname, ".%s.3", file);
	            		} if (pt == 3) {
	            			bzero(chunkname, MAXBUF);
		    				sprintf(chunkname, ".%s.4", file);
	            		}
	            		if (serverToReq == 0) {
		            		strcpy(ip, server.ip1);
			    			portno = server.port1;
			    		} else if (serverToReq == 1) {
		            		strcpy(ip, server.ip2);
			    			portno = server.port2;
			    		} else if (serverToReq == 2) {
		            		strcpy(ip, server.ip3);
			    			portno = server.port3;
			    		} else if (serverToReq == 3) {
		            		strcpy(ip, server.ip4);
			    			portno = server.port4;
			    		}

	            		// send get request to server
	            		socket = connect_to_server(ip, portno);
						if (socket < 0) {
							printf("Connection to DFS%d failed\n", serverToReq + 1);
							printf("File is incomplete.\n");
							fclose(fptr);
							// clean up ??
						} else {
							bzero(buf, MAXBUF);
							sprintf(buf, "get %s %s %s", server.username, server.password, chunkname);
							n = write(socket, buf, strlen(buf));
							if (is_auth(socket)) {
								// read in file contents
								while (1) {
			                		bzero(buf, MAXBUF);
			                		n = read(socket, buf, MAXBUF);
			                		if (!strncmp(buf, "END", 3)) {
			                			break;
					                }
					                //// DECRYPT /////
					                for (int i = 0; i < n; i ++) {
					                	buf[i] = buf[i] - 1 ;
					                }
					                //////////////////
					                // write to server
					                fwrite(buf, 1, n, fptr);
					            }
					        }
			            }
	            	}
	            	fclose(fptr);
	            }
	        } else {
	        	printf("File not found\n");
	        }
        }
        /* 
		put - uploads file onto DFSs 
		*/
		else if (!strncmp(uinput, "put", 3)) {
			// check if file exists
			FILE* fp = fopen(file, "rb");
			if (fp == NULL) {
        		printf ("%s can't be opened.\n", file);
    		} else {
			    // create chunk filenames
    			bzero(chunk1name, MAXBUF);
    			sprintf(chunk1name, ".%s.1", file);
    			bzero(chunk2name, MAXBUF);
    			sprintf(chunk2name, ".%s.2", file);
    			bzero(chunk3name, MAXBUF);
    			sprintf(chunk3name, ".%s.3", file);
    			bzero(chunk4name, MAXBUF);
    			sprintf(chunk4name, ".%s.4", file);

    			//Calculate file size
	      		fseek(fp, 0, SEEK_END);
	      		int fsize = ftell(fp);
	      		printf("size %d\n", fsize);
	      		rewind(fp);
	      		// Break file into 4 evenly sized chunks
	      		int chunksize = fsize/4;
	      		int chunk1offset = 0;
	      		int chunk2offset = chunksize;
	      		int chunk3offset = chunk2offset + chunksize;
	      		int chunk4offset = chunk3offset + chunksize;
	      		int chunk4size = fsize - chunk4offset; // Whatever is remaining

	      		// hash file
		      	int hash_bucket = hash_mod(file);
		      	printf("hash %d\n", hash_bucket);
	      		for (int i = 0; i < 4; i++ ){
      				if (i == 0) {
  						if (hash_bucket == 0) {
  							// DFS1 gets chunk 1 and 2
  							// chunk1 
  							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  							// chunk 2
  							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  						} else if (hash_bucket == 1) {
  							// DFS1 gets chunk 4 and 1
  							// chunk1 
  							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  							// chunk4
  							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  						} else if (hash_bucket == 2) {
  							// DFS1 gets chunk 3 and 4
  							// chunk 3
  							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  						} else if (hash_bucket == 3) {
  							// DFS1 gets chunk 2 and 3
  							// chunk 2
  							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  							// chunk 3
  							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
						}
      				} else if (i == 1) {
  						if (hash_bucket == 0) {
  							// DFS2 gets chunk 2 and 3
  							// chunk 2
  							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
      						// chunk 3
      						socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  						} else if (hash_bucket == 1) {
  							// DFS2 gets chunk 1 and 2
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  							// chunk 2
  							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  						} else if (hash_bucket == 2) {
  							// DFS2 gets chunk 4 and 1
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  						} else if (hash_bucket == 3) {
  							// DFS2 gets chunk 3 and 4
  							// chunk 3
							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  						}
      				} else if (i == 2) {      					
						if (hash_bucket == 0) {
  							// DFS3 gets chunk 3 and 4
  							// chunk 3
							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  						} else if (hash_bucket == 1) {
  							// DFS3 gets chunk 2 and 3
  							// chunk 2
							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  							// chunk 3
							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  						} else if (hash_bucket == 2) {
  							// DFS3 gets chunk 1 and 2
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  							// chunk 2
							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  						} else if (hash_bucket == 3) {
  							// DFS3 gets chunk 4 and 1
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  						}
      				} else if (i == 3) {					
  						if (hash_bucket == 0) {
  							// DFS4 gets chunk 4 and 1
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  						} else if (hash_bucket == 1) {
  							// DFS4 gets chunk 3 and 4
  							// chunk 3
							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  							// chunk4
							socks[3] = send_chunk(i, fp, server, chunk4name, chunk4offset, chunksize);
  						} else if (hash_bucket == 2) {
  							// DFS4 gets chunk 2 and 3
  							// chunk 2
							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  							// chunk 3
							socks[2] = send_chunk(i, fp, server, chunk3name, chunk3offset, chunksize);
  						} else if (hash_bucket == 3) {
  							// DFS2 gets chunk 1 and 2
  							// chunk1 
							socks[0] = send_chunk(i, fp, server, chunk1name, chunk1offset, chunksize);
  							// chunk 2
							socks[1] = send_chunk(i, fp, server, chunk2name, chunk2offset, chunksize);
  						}
      				}
				}
    		}
        }
		/* 
		list - print file names under the Username directory on DFS servers.
		identify if file pieces on DFS servers are enough to reconstruct the original file
		*/
        else if (!strncmp(uinput, "ls", 2)) {
      		update_file_info(server);
      		printf("\nFile list:\n");
      		for (int i = 0; i < files.size(); i++) {
      			printf("%s", files.at(i).filename);
      			for (int r = 0; r < 4; r++) {
                	if ((files.at(i).chunks[r][0] == 0) && (files.at(i).chunks[r][1] == 0) && (files.at(i).chunks[r][2] == 0) && (files.at(i).chunks[r][3] == 0)) {
            			printf(" [incomplete]");	
            			break;
            		}
                }
            	printf("\n");
            }
        	
		} else {
        	printf("Error: Invalid request\n");
        }
    }
}

void update_file_info(server_info server) {
	char buf[MAXBUF];
	char name[MAXBUF];
	int n, socket, filefound, portno;
	char ip[MAXIP];

	files.clear();
	// get file list from DFS1
	for (int i = 0; i < 4; i++) {
		if (i == 0) {
			socket = connect_to_server(server.ip1, server.port1);
		} else if (i == 1) {
			socket = connect_to_server(server.ip2, server.port2);
		} else if (i == 2) {
			socket = connect_to_server(server.ip3, server.port3);
		} else if (i == 3) {
			socket = connect_to_server(server.ip4, server.port4);
		}
    	if (!(socket<0)) {
    		sprintf(buf, "ls %s %s", server.username, server.password);
    		n = write(socket, buf, strlen(buf));
			if (n < 0) {
				printf("ERROR in write to DFS%d\n", i);
			} else {
				if (is_auth(socket)) {
					printf("Receiving file list...\n");
					// get file info 
					while (1) {
						bzero(buf, MAXBUF);
						bzero(name, MAXBUF);
		                n = read(socket, buf, MAXBUF);
		                //printf("rec %s\n", buf);
		                if (!strncmp(buf, "END", 3) || (n<=0)) {
		                	break;
		                }
		                int num = (int)buf[0] - 48; // ascii hack
		                char *p = &buf[1];
		                strcpy(name, p);
		                filefound = 0;
	                	for (int j = 0; j < files.size(); j++) {
	                		if (!strcmp(files.at(j).filename, name)) {
	                			filefound = 1;
	                			files.at(j).chunks[num-1][i] = 1;
	                			break;
	                		}
	                	}
	                	if (!filefound && (strlen(buf) > 0)) {
	                		file_info newFile;
	                		strcpy(newFile.filename, name);
	                		newFile.chunks[num-1][i] = 1;
	                		files.push_back(newFile);
	                	}
	                }
	            }
	        }
	    	close(socket);
		} else {
			printf("Connection to DFS%d failed\n", i + 1);
			// mark files on failed server unavailable
			for (int k = 0; k < files.size(); k++) {
				for (int j = 0; j < 4; j++) {
					files.at(k).chunks[j][i] = 0;
				}
			}
		}
    }
}


/*
Sends put command do server and calls put_on_server()
*/
int send_chunk(int servernum, FILE *fp, server_info server, char *chunkname, int chunkoffset, int chunksize) {
	char buf[MAXBUF];
	int n, socket;
	if (servernum == 0) {
		socket = connect_to_server(server.ip1, server.port1);
	} else if (servernum == 1) {
		socket = connect_to_server(server.ip2, server.port2);
	} else if (servernum == 2) {
		socket = connect_to_server(server.ip3, server.port3);
	} else if (servernum == 3) {
		socket = connect_to_server(server.ip4, server.port4);
	}
	if (socket < 0) {
		printf("Connection to DFS%d failed\n", servernum + 1);
		return -1;
	} else {
		bzero(buf, MAXBUF);
		// send put command
		sprintf(buf, "put %s %s %s", server.username, server.password, chunkname);
		n = write(socket, buf, strlen(buf));
		if (n < 0) {
			printf("ERROR in write to DFS%d\n", servernum + 1);
		} else {
			// send file
			put_on_server(chunkoffset, chunksize, fp, socket, servernum);
		}
		return 0;
	}
}

/*
Connect the proxy to the server requested and return socket
*/
int connect_to_server(char* hostname, int portno){
	int serversk, optval=1;
	struct sockaddr_in serveraddr;

	// Find the server address associated with the hostname
	/* gethostbyname: get the server's DNS entry */
    struct hostent *server = gethostbyname(hostname);
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
	    serveraddr.sin_port = htons(portno);
	    int serverlen = sizeof(serveraddr);

	    // Connect to the server
	    if (connect(serversk, (struct sockaddr*)&serveraddr, serverlen)<0){
	    	return -1;
	    }
	    return serversk;
	}
	return -1;
}

// Checks if user is authorized by username and password credentials
int is_auth(int socket) {
	char buf[MAXBUF];
	int n = read(socket, buf, MAXBUF);
	if (n < 0) {
		printf("ERROR in read from server\n");
	}
	if (!strncmp(buf, "OK", 2)){ // Authenticated
		return 1;
	} else {
		printf("Error - User authentication failed\n");
		return 0;
	}
}

/*
sends file contents to server - main functionality of put command
*/
void put_on_server(int offset, int chunksize, FILE *fp, int socket, int servernum) {
	char buf[MAXBUF];
	int rec;
	if (is_auth(socket)) { // Authenticated
		fseek (fp, offset, SEEK_SET);
		while (1) {
			bzero(buf, MAXBUF);
			if (chunksize <= MAXBUF) {
				rec = fread(buf, 1, chunksize, fp);
			} else{
				rec = fread(buf, 1, MAXBUF, fp);
			}
	    	if (rec <= 0) {
	    		break;
	    	}
	    	//// ENCRYPT /////
	    	for (int i = 0; i < rec; i ++) {
	    		buf[i] = buf[i] + 1;
	    	}
	    	//////////////////
	    	// write to client
	    	write(socket, buf, rec);
	    	// increment remaining write size
	    	chunksize = chunksize - rec;
	    }
	    bzero(buf, MAXBUF);
		sprintf(buf, "END");
		// write end message to server
		printf("Sending file chunk to DFS%d...\n", servernum+1);
		sleep(1);
		write(socket, buf, MAXBUF);
		rewind(fp);
	}
	close(socket);
}

// using openSSL md5 hash from https://www.openssl.org/docs/man1.0.2/man3/md5.html
int hash_mod(char *fname) {
	int bytes;
	uint32_t num;
	MD5_CTX context;
	unsigned char digest[MD5_DIGEST_LENGTH];
	unsigned char data[1024];

	FILE *fp = fopen (fname, "rb");
	if (fp == NULL) {
        printf ("%s can't be opened.\n", fname);
        return -1;
    }
    MD5_Init(&context);
    while ((bytes = fread(data, 1, 1024, fp)) != 0) {
    	MD5_Update(&context, data, bytes);
    }
    MD5_Final(digest, &context);
	memcpy(&num, digest, 4);
	num = (num % 4);
    return num;
}
