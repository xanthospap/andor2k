#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>
#ifdef SOCKET_LOGGER
#include <cassert>
#include <chrono>
#include <ctime>
#endif
#include <thread>
#include <pthread.h>
#include <chrono>
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

int status_error = 0;
void *getStatus(void *ptr) noexcept {
  status_error = 0;
  ptr = nullptr;
  if (ptr) printf("why is this not null?\n");
  
  try {
    ClientSocket status_socket("localhost", 8080+1);
    printf("Created client status socket\n");

    char buf[1024];
    while (true) {
      ::bzero(buf, sizeof(buf));
      std::strcpy(buf, "status");
      status_socket.send(buf);
      ::bzero(buf, sizeof(buf));
      status_socket.recv(buf, 1024);
      printf("\nGot string from server: \"%s\"", buf);
      std::this_thread::sleep_for(1200ms);
    }

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
    status_error = 1;
    return (void*)(&status_error);
  }
}

int main() {
  //pthread_t status_thread;
  //int status_thread_error = pthread_create(&status_thread, NULL, getStatus, NULL);

  try {
    // create and connect ....
    printf("Creating client socket ... connecting to localhost at 8080 ...");
    ClientSocket client_socket("127.0.0.1", 8080);
    
    // chat with server via the socket
    printf(" ready! can now talk to server side.\n");
    chat(client_socket);
  
  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
    // fprintf(stderr, "%s", e.what().c_str());
  }

  printf("All done!\n");
  return 0;
}
