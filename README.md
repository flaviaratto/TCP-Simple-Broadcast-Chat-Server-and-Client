# TCP-Simple-Broadcast-Chat-Server-and-Client
Implementation of the client and server for a simple chat service.


ECEN602 Programming Assignment 2
1. Flavia Ratto
2. Eric Lloyd Robles

## Assignment Overview
In this assignment, we implemented the client and server for a simple chat service. The Simple Broadcast Chat Protocol (SBCP) is a protocol that allows clients to join and leave a global chat session, view members of the session, and send and receive messages
* An instance of the server provides a single “chat room,” which can only handle a finite number of clients. 
* Clients must explicitly JOIN the session. A client receives a list of the connected members of the chat session once they complete the JOIN transaction. 
* Clients use SEND messages to carry chat text, and clients receive chat text from the server using the FWD message. 
* Clients may exit unceremoniously at any time during the chat session. 
* The server should detect a client exit, cleanup the resources allocated to that client and notify the other clients. 
* Also, both the client and server handle several inputs at the same time using I/O multiplexing (select).

## Steps to compile and run

1. In order to compile the code, make sure the makefile, client.c and server.c are in the same directory/folder/. Type in the command:
    ```
    make
    ```
2. To start the server, type in the command line:
    ```
    ./server server_ip server_port max_clients
    ```
3. To start the client, type in the command line: ./client IPAdr Port
    ```
    ./client username server_ip server_port
    ```

**Note:** - The code is written in C and is compiled and tested in a Linux environment.
