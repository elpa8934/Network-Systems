Network Systems - CSCI 4273
PA #4 - Disributed File System
Liz Parker

This is a basic distributed file system with 1 client and put to 4 servers. Each file is into pieces and stored on different servers for redundancy and reliability. 

Setup:
The server must be in a folder containing a dfs.conf file and 4 directories called DFS1, DFS2, DFS3, and DFS4. 
The .conf files must be configured properly (examples included)

To run:
inside both client and server folder there is a makefile to compile the c++ code. 
To run the client, use:
	./client dfc.conf
To run a server, use the format
	./server /DFS<num> <port>
for example:
	./server /DFS1 10001

Extra credit: 
Traffic optimization
	Instead of downloading all available file fragments, my client first surveys the servers to see what is available. If not all 4 of the pieces are available, then no fragments will be downloaded. Otherwise, only 4 fragments will be downloaded from available servers. 

Encryption:
	I implemented an extremely simple encryption of the file data that simply added 1 to the value of each character in the file before sending to the server, then subtracted 1 from the value of each character before writing back to the client. I was able to observe the effective concealement in WireShark. 