#include "andor2k.hpp"
#include "cpp_socket.hpp"
#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <ctime>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>
#include <cstring>

using andor2k::ClientSocket;
using andor2k::Socket;

constexpr char ANDOR_DAEMON_HOST[] = "127.0.0.1";
constexpr int ANDOR_DAEMON_PORT = 8080;

void chat(const ClientSocket &socket) {
  char buffer[1024];
  
  for (;;) {
    
    // get string from user
    std::memset(buffer, '\0', sizeof(buffer));
    printf("\nEnter the string: ");
    int n = 0;
    while ((buffer[n++] = getchar()) != '\n')
      ;
    
    // send message to client
    socket.send(buffer);
    
    // read message from server
    std::memset(buffer, '\0', sizeof(buffer));
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
    printf("[DEBUG] Trying to connect to the andor2k daemon ...\n");
    ClientSocket client_socket(ANDOR_DAEMON_HOST, ANDOR_DAEMON_PORT);
    
    // chat with server via the socket
    printf("[DEBUG] Connection with daemon established; type commands\n");
    chat(client_socket);
  
  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
    fprintf(stderr, "[ERROR] Failed to connect to andor daemon; is it up and running?\n");
    fprintf(stderr, "[FATAL] Exiting\n");
    return 1;
    // fprintf(stderr, e.what());
  }

  printf("All done!\n");
  return 0;
}