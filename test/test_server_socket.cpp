#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>

using aristarchos::ServerSocket;
using aristarchos::Socket;

void chat(const Socket &socket) {
  char buffer[1024];
  for (;;) {
    ::bzero(buffer, sizeof(buffer));
    // read message from client into buffer
    socket.recv(buffer, 1024);
    // print client message
    printf("Got string from client: \"%s\"", buffer);
    // get message for client
    printf("Message for client:");
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
  aristarchos::SocketLogger log("server_socket.log");
#endif

  try {
#ifdef SOCKET_LOGGER
    ServerSocket server_sock(8080, &log);
#else
    ServerSocket server_sock(8080);
#endif

    Socket child_socket = server_sock.accept(status);
    if (status < 0) {
      std::cerr << "\n[ERROR] Failed to create child socket!";
      return 1;
    }

    chat(child_socket);

  } catch (std::exception &e) {
    std::cerr << "\n[ERROR] Exception caught!";
    std::cerr << "\n" << e.what();
  }

  std::cout << "\nAll done!";
  return 0;
}
