#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>

using andor2k::ServerSocket;
using andor2k::Socket;

void chat(const Socket &socket) {
  char buffer[1024];
  for (;;) {
    ::bzero(buffer, sizeof(buffer));
    // read message from client into buffer
    socket.recv(buffer, 1024);
    // print client message
    printf("Got string from client: %s", buffer);
    // get message for client
    printf("Enter message for client:");
    int n = 0;
    ::bzero(buffer, sizeof(buffer));
    while ((buffer[n++] = getchar()) != '\n')
      ;
    // send message to client
    socket.send(buffer);
    // if message contains "exit" end chat
    if (std::strncmp("exit", buffer, 4) == 0) {
      printf("Server Exit ...\n");
      break;
    }
  }
  return;
}

int main() {
  int status;
#ifdef SOCKET_LOGGER
  printf("Note: Creating socket log file \"server_socket.log\"\n");
  andor2k::SocketLogger log("server_socket.log");
#endif

  try {
#ifdef SOCKET_LOGGER
    ServerSocket server_sock(8080, &log);
#else
    ServerSocket server_sock(8080);
#endif

    Socket child_socket = server_sock.accept(status);
    if (status < 0) {
      fprintf(stderr, "[ERROR] Failed to create child socket!\n");
      return 1;
    }

    printf("Server Socket created; client atached and ready!\n");
    chat(child_socket);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
    // fprintf(stderr, e.what());
  }

  printf("All done!\n");
  return 0;
}
