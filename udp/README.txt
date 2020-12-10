Liz Parker
CSCI 4273 Network Systems
Programming Assignment 1
UDP Socket Programming

This program contains a basic implementation of a packet delivery system using UDP. 
It is unreliable, and some packets may be lost. 
Server and client know when to stop listening  when the an "END" packet is sent. 

To run the client, navigate inside the client folder
call make
call ./client [host IP] [port number]

To run the server, navigate inside the server folder
call make
call ./server [port number]

