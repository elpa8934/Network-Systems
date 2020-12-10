Network Systems - CSCI 4273
PA #3 - HTTP Proxy Server
Liz Parker
11/22/2020

proxy.c implements simple http proxy server with multithreading, caching, and blacklisting. 

To compile the http proxy server, enter the proxy/ directory and type:
	make
To run the http server, type:
	./proxy <PORTNO> [timeout optional]
For example:
	./proxy 48888
	./proxy 48888 30

Run the http proxy server
	
	Rake a local request from the terminal. For example:
	(echo -en "GET http://netsys.cs.colorado.edu HTTP/1.0") | nc 127.0.0.1 48888

	OR

	Configure a browser to use the proxy on the specified port

When requests are made for the same content within the timeout period, the server will access the file from cache. The cache files are stored on the proxy server, and the location of the files are stored in a vector of cache objects that record the file location, hostname, and the file's last call for timeout purposes. The cache data structure removes timed out files whenever it is used. 

To blacklist a service, add the hostname or IP address to its own line in the blacklist file. Each file requested by the client will be checked against the blacklist before being serviced. 



