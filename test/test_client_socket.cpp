#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>
#ifdef SOCKET_LOGGER
#include <cassert>
#include <chrono>
#include <ctime>
#endif

using aristarchos::ClientSocket;

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
#ifdef SOCKET_LOGGER
  aristarchos::SocketLogger log("client_socket.log");
#endif

  try {
    // create and connect ....
#ifdef SOCKET_LOGGER
    ClientSocket client_socket("127.0.0.1", 8080, &log);
#else
    ClientSocket client_socket("127.0.0.1", 8080);
#endif
    // chat with server via the socket
    chat(client_socket);
  } catch (std::exception &e) {
    std::cerr << "\n[ERROR] Exception caught!";
    std::cerr << "\n" << e.what();
  }

  std::cout << "\nAll done!";
  return 0;
}
