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
