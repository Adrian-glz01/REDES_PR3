//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include "common.h"
#include "ClientConnection.h"
#include "FTPServer.h"

// Constructor
ClientConnection::ClientConnection(int s) {
  int sock = (int)(s);

  char buffer[MAX_BUFF];

  control_socket = s;
  // Check the Linux man pages to know what fdopen does.
  fd = fdopen(s, "a+");
  if (fd == NULL) {
    std::cout << "Connection closed\n\n";
    fclose(fd);
    close(control_socket);
    ok = false;
    return;
  }

  ok = true;
  data_socket = -1;
  stop_srv = false;
};

// Destructor
ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket); 
}

int connect_TCP(uint32_t address, uint16_t port) {
  struct sockaddr_in client_addr;
  int sock;

  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(port);
  client_addr.sin_addr.s_addr = address;

  if (client_addr.sin_addr.s_addr == INADDR_NONE) {
    std::cerr << "Invalid IP address" << std::endl;
    return -1;
  }
  
  sock = socket(AF_INET, SOCK_STREAM, 0);  // Create the socket
  if (sock < 0) {
    std::cerr << "Error creating socket" << std::endl;
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
    std::cerr << "Error connecting to server" << std::endl;
    return -1;
  }
  return sock;
}

void ClientConnection::stop() {
    close(data_socket);
    close(control_socket);
    stop_srv = true;
  
}
    
#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
  if (!ok) {
    return;
  }

  fprintf(fd, "220 Service ready\n");
  bool logged = false;

  while (!stop_srv) {
    fscanf(fd, "%s", command);
    if (COMMAND("USER")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "331 User filename ok, need password\n");
    }
    else if (COMMAND("PWD")) {
      if (logged) {
        fprintf(fd, "257 \"%s\" is current directory\n", getcwd(NULL, 0));
      }
      else {
        fprintf(fd, "530 Not logged in\n");
      }
    }
    else if (COMMAND("PASS")) {
      fscanf(fd, "%s", arg);
      if (strcmp(arg, "1234") == 0) {
        fprintf(fd, "230 User logged in\n");
        logged = true;
      }
      else {
        fprintf(fd, "530 Not logged in\n");
        stop_srv = true;
      }
    }
    else if (COMMAND("PORT")) {
      int addr[4];
      int prt[2];
      fscanf(fd, "%d,%d,%d,%d,%d,%d", &addr[0], &addr[1], &addr[2], &addr[3], &prt[0], &prt[1]);
      uint32_t address = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
      uint16_t port = (prt[0] << 8) | (prt[1] << 0);
      data_socket = connect_TCP(address, port);

      if (data_socket < 0) {
        fprintf(fd, "425 Can't open data connection\n");
      }
      else {
        fprintf(fd, "200 Data connection open\n");
      }
    }
    else if (COMMAND("PASV")) {
      if (logged) {
        struct sockaddr_in sin;
        socklen_t slen = sizeof(struct sockaddr_in);

        int msock = define_socket_TCP(0);  // Create the socket                                 
        int sock_port = getsockname(msock, (struct sockaddr *)&sin, &slen);  // Get the port number
        if (sock_port | msock < 0) {
          fprintf(fd, "421 Service not available, closing control connection\n");
        }
        uint16_t port = sin.sin_port;  
        uint16_t port_1 = (port >> 8) & 0xFF;
        uint16_t port_2 = port & 0xFF;

        fprintf(fd, "227 Entering Passive Mode (127,0,0,1,%d,%d)\n", port_2, port_1);
        fflush(fd);
        data_socket = accept(msock, (struct sockaddr *)&sin, &slen);  // Wait for the client to connect
        if (data_socket < 0) {
          fprintf(fd, "421 Service not available, closing control connection\n");
        }
      }
      else {
        fprintf(fd, "530 Not logged in\n");
      } 
    }
    else if (COMMAND("STOR")) { 
      if(logged) {
        char buff[MAX_BUFF]; 
        int bytes_recibidos;
        size_t len = sizeof(buff);
        fscanf(fd, "%s", arg);                
        FILE * data_fd = fopen(arg, "wb");  
        if ( data_fd == NULL) {
            fprintf(fd, "425 Can't open data connection.\n");
        }
        fprintf(fd, "150 File creation okay; about to open data connection.\n");
        fflush(fd);
        while (true) {
          bytes_recibidos = recv(data_socket, buff , len, 0); 
          fwrite( buff,  1, bytes_recibidos , data_fd); 
          if ( bytes_recibidos < MAX_BUFF) {
            break;
          }
                           
        }
        fprintf(fd, "226 Closing data connection.\n");
        close(data_socket);
        fclose(data_fd);
      } else {
        fprintf(fd, "530 Please login with USER and PASS.\n");
      }        
    }
    else if (COMMAND("RETR")) {
      if(logged) {
        char buff[MAX_BUFF];        
        int sent_bytes;
        fscanf(fd, "%s", arg);

        int file = open(arg, O_RDONLY);
        if (file < 0) {
          fprintf(fd, "425 Can't open data connection.\n");
        }
        fprintf(fd, "150 File status okay; about to open data connection.\n");
        
        while (1) {
          sent_bytes = read(file, buff, MAX_BUFF); 
          if (sent_bytes == 0){
            break;
          }
          send(data_socket, buff, sent_bytes, 0); 
        }
        fprintf(fd, "226 Closing data connection.\n");
        close(file);
        close(data_socket);
      } 
      else {
        fprintf(fd, "530 Not logged in\n");
      }
    }
    else if (COMMAND("LIST")) {
      if (logged) {
        std::string filename;
        DIR *dir;
        
        dir = opendir(".");
        if (dir == NULL) {
          fprintf(fd, "450 Requested file action not taken.\n");
        }
        fprintf(fd, "150 List started OK\n");

        struct dirent* directory = readdir(dir);
        while (directory != NULL) {               
          if ((strcmp(directory->d_name, ".") != 0) && (strcmp(directory->d_name, "..") != 0)) {
            filename += directory->d_name;
            filename += "\r\n";
          }
          directory = readdir(dir);
        }
        send(data_socket, filename.c_str(), filename.size(), 0);
        closedir (dir);
        fprintf(fd, "226 List completed successfully.\n");
        close(data_socket);
      } 
      else {
       fprintf(fd, "530 Please login with USER and PASS.\n");
      }
   }
    else if (COMMAND("SYST")) {
      fprintf(fd, "215 UNIX Type: L8.\n");
    }

    else if (COMMAND("TYPE")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "200 OK\n");
    }

    else if (COMMAND("QUIT")) {
      fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
      close(data_socket);
      stop_srv = true;
      break;
    }

    else {
      fprintf(fd, "502 Command not implemented.\n");
      fflush(fd);
      printf("Comando : %s %s\n", command, arg);
      printf("Error interno del servidor\n");
    }
  }
  fclose(fd);

  return;
};