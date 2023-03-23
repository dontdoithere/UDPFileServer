# UDP File Transfer Server

 This is a simple UDP file transfer server that allows clients to download files. The server receives requests from clients with a filename, reads the file, and sends it back to the client in blocks of fixed size. The server is written in C language and consists of two files:
 
    - UDPClient.c - the client application
    - ServerUDP.c - the server application
    
    
## UDPClient.c

The client application is used to request files from the server. To use the client application, you need to provide the hostname of the server as a command-line argument. Once the client is running, you can enter the filename you want to download. The client will download the file and save it to a temporary file on disk.

### Dependencies
-    readline library

### Usage
$ ./UDPClient <hostname>
 
 
## ServerUDP.c
 
The server application listens for incoming requests from clients and responds with the requested file. The server is designed to handle only one client at a time. If another client connects while the server is already handling a request, the new request is ignored. The server reads the requested file, splits it into blocks of fixed size, and sends each block to the client. The client assembles the blocks to reconstruct the file.
 
### Usage

 $ ./ServerUDP
 
 ## Code Explanation:
 
 Both the client and server applications use the User Datagram Protocol (UDP) to transfer data. UDP is a connectionless protocol that does not guarantee delivery of data or ensure that data is delivered in the order in which it was sent. Therefore, the client and server need to handle lost or out-of-order data.

The server application is designed to handle only one client at a time. To handle multiple clients simultaneously, the server application needs to be modified to use threading or forking.

The client application uses a linked list to store the received blocks of data before saving them to disk. This allows the client to assemble the blocks in the correct order even if they arrive out of order.

The server application sets a receive timeout for the socket to avoid blocking indefinitely if a client does not send any data. If no data is received before the timeout expires, the server continues to the next iteration of the loop.

The server application uses the signal() function to handle broken pipes caused by disconnected clients. The signal() function registers a function to be called when a specific signal is received. In this case, the function is called when a SIGPIPE signal is received, which indicates that the client has disconnected.

The server application uses the pthread library to create a new thread to handle each client request. This allows the server to handle multiple clients simultaneously.
