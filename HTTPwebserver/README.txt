Network Systems - CSCI 4273
PA #2 - HTTP Server
Liz Parker
10/18/2020

httpsetver.c implements a simple http web server. 

To compile the http server, enter the www/ directory and type:
	make
To run the http server, type:
	./server <PORTNO>
For example:
	./server 48888

Local intructions
	Run the http server locally
	To access GET locally, open browser and type url:
		http://localhost:<PORTNO>
	For example: 
		http://localhost:48888
	Feel free to click around and load different types of files

	To test POST locally, open a terminal and type:
		(echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | nc 127.0.0.1 <PORTNO>
	For example:
		(echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | nc 127.0.0.1 <PORTNO>
	Examine written packet with header and data matching:
		HTTP/1.1 200 OK
		Content-type: text/html
		Content-size: 3391
		<html><body><pre><h1>POSTDATA </h1></pre><!DOCTYPE html PUBLIC "-//W3C//DTD
		XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
		<html xmlns="http://www.w3.org/1999/xhtml> so on and the rest of your HTML file.


CU CS remote server instructions
	Run the http server on the remote server
	To access GET remotely, open browser and type url:
		http://elra-<NUM>.cs.colorado.edu:<PORTNO>
	For example: 
		http://elra-01.cs.colorado.edu:<PORTNO>
	Feel free to click around and load different types of files

	To test POST remotely, open a in the remote server terminal and type:
		(echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | nc 127.0.0.1 <PORTNO>
	For example:
		(echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | nc 127.0.0.1 <PORTNO>
	Examine written packet with header and data matching:
		HTTP/1.1 200 OK
		Content-type: text/html
		Content-size: 3391
		<html><body><pre><h1>POSTDATA </h1></pre><!DOCTYPE html PUBLIC "-//W3C//DTD
		XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
		<html xmlns="http://www.w3.org/1999/xhtml> so on and the rest of your HTML file.