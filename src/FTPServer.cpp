//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//                        Main class of the FTP server
// 
//****************************************************************************

#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <pthread.h>

#include <list>

#include <iostream>

#include "common.h"
#include "FTPServer.h"
#include "ClientConnection.h"

int define_socket_TCP(int port) {
  struct sockaddr_in serv_addr;
  int sock = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
  if (sock < 0) { // Error control
    std::cerr << "Error creating socket\n";
    return -1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr)); // Zero the structure
  serv_addr.sin_family = AF_INET;           // Internet address family
  serv_addr.sin_addr.s_addr = INADDR_ANY;   // Any incoming interface
  serv_addr.sin_port = htons(port);         // Local port

  if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Error control
    std::cerr << "Error binding socket\n";
    return -1;
  }

  if (listen(sock, 5) < 0) { // Error control
    std::cerr << "Error listening socket\n";
    return -1;
  }
  return sock; // Return socket descriptor
}

// This function is executed when the thread is executed.
void* run_client_connection(void *c) {
    ClientConnection *connection = (ClientConnection *)c;
    connection->WaitForRequests();
  
    return NULL;
}


// Change the port of the server
FTPServer::FTPServer(int port) { this->port = port; }

// Stop the server
void FTPServer::stop() {
  close(msock);
  shutdown(msock, SHUT_RDWR);
}

// Starting of the server
void FTPServer::run() {
  struct sockaddr_in fsin;
  int ssock;
  socklen_t alen = sizeof(fsin);
  msock = define_socket_TCP(port);
  if (msock < 0) {
    errexit("Error defining the socket: %s\n", strerror(errno));
  }

  while (1) {
	  pthread_t thread;
    ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
    if(ssock < 0) {
      errexit("Error in accept function: %s\n", strerror(errno));
    }

	  ClientConnection *connection = new ClientConnection(ssock);
	
	  // Here a thread is created in order to process multiple requests simultaneously
	  pthread_create(&thread, NULL, run_client_connection, (void*)connection);    
  }
}