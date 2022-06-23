#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>
#ifdef SOCKET_LOGGER
#include <cassert>
#include <chrono>
#include <ctime>
#endif
#include <chrono>
#include <pthread.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using andor2k::ClientSocket;

void chat(const ClientSocket &socket) {
  
  char buffer[1024];
  for (;;) {
    
    ::bzero(buffer, sizeof(buffer));
    // get string from user
    printf("\nEnter the string: ");
    int n = 0;
    while ((buffer[n++] = getchar()) != '\n')
      ;
    
    // send message to client
    // remove the trailing newline; it hits me in the nerves!
    buffer[--n] = '\0';
    socket.send(buffer);
    
    // read message from server
    socket.recv(buffer, 1024);
    printf("\nGot string from server: \"%s\"", buffer);
    
    // if message contains "exit" then exit chat
    if (std::strncmp("exit", buffer, 4) == 0) {
      printf("Client exit ...\n");
      break;
    }
  }
  
  return;
}

int main() {

  try {
    // create and connect ....
    printf("Creating client socket ... connecting to localhost at 8080 ...");
    ClientSocket client_socket("127.0.0.1", 8080);

    // chat with server via the socket
    printf(" ready! can now talk to server side.\n");
    chat(client_socket);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught/Closing socket (client)\n");
  }

  printf("All done!\n");
  return 0;
}
