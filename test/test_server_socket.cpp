#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>
#include <thread>
#include <pthread.h>
#include <chrono>
#include <unistd.h>

using namespace std::chrono_literals;
using andor2k::ServerSocket;
using andor2k::Socket;

char buff_main[1024];
char buff_stat[1024];
int status_th_error;
int doing_work = 0;
int sabort = 0;

void do_work() {
  doing_work = 1;
  printf("server doing work ....\n");
  for (int i=0; i<20; ++i) {
    std::this_thread::sleep_for(3000ms);
    printf("\tworking ... for function: %s\n", __func__);
  }
  doing_work = 0;
  return;
}

void chat(const Socket &socket) {
  for (;;) {
    ::bzero(buff_main, sizeof(buff_main));
    // read message from client into buff_main
    socket.recv(buff_main, 1024);
    
    // print client message
    printf("Got string from client: %s", buff_main);
    if (!std::strncmp("shutdown", buff_main, 8)) break;

    // do some work
    do_work();
    
    // get message for client
    printf("Enter message for client:");
    int n = 0;
    ::bzero(buff_main, sizeof(buff_main));
    while ((buff_main[n++] = getchar()) != '\n')
      ;
    
    // send message to client
    socket.send(buff_main);
  }
  return;
}

void *statusSockFunc(void *ptr) noexcept {
  int error = 0;
  pid_t ipid = getpid();
  status_th_error = 0;
  ptr = nullptr;
  if (ptr) printf("why is this not null?\n");

  try {
    ServerSocket server_sock(8080+1);
    Socket child_socket = server_sock.accept(error);
    if (error < 0) {
      fprintf(stderr, "[ERROR] Failed to create child socket (in status thread)!\n");
      // return 1;
      status_th_error = 1;
      return (void*)(&status_th_error);
    }
    printf("Status Server Socket created; client atached and ready!\n");
    // let's chat!
    while (true) {
      ::bzero(buff_stat, sizeof(buff_stat));
      // read message from client into buff_stat
      child_socket.recv(buff_stat, 1024);
      
      // do not print client message
      // printf("Got string from client: %s (status socket)", buff_stat);
      if (!std::strncmp("shutdown", buff_stat, 8)) break;
      
      // message for client is the PID
      int sz = 0;
      ::bzero(buff_stat, sizeof(buff_stat));
      std::strcpy(buff_stat+sz, "PID: ");
      sz += 5;
      sz += sprintf(buff_stat+sz, "%d", static_cast<int>(ipid));
      std::strcpy(buff_stat+sz, " WORKING: ");
      sz += std::strlen(" WORKING: ");
      sz += sprintf(buff_stat+sz, "%d", static_cast<int>(doing_work));
      std::strcpy(buff_stat+sz, " ABORT: ");
      sz += std::strlen("ABORT: ");
      sz += sprintf(buff_stat+sz, "%d", sabort);
      
      // send message to client
      child_socket.send(buff_stat);
    }
  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught (status socket)!\n");
    status_th_error = 1;
    return (void*)(&status_th_error);
  }

  return (void*)(&status_th_error);
}

int main() {
  int status;
#ifdef SOCKET_LOGGER
  printf("Note: Creating socket log file \"server_socket.log\"\n");
  andor2k::SocketLogger log("server_socket.log");
#endif

  // create a socket dedicated to listen for status
  pthread_t status_thread;
  int thread_error = pthread_create(&status_thread, NULL, statusSockFunc, NULL);

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

    printf("Main Server Socket created; client atached and ready!\n");
    // Chat with main socket
    /*for (;;) {
      ::bzero(buff_main, sizeof(buff_main));
      // read message from client into buff_main
      child_socket.recv(buff_main, 1024);
      
      // print client message
      printf("Got string from client: %s", buff_main);
      if (!std::strncmp("shutdown", buff_main, 8)) break;
      
      // get message for client
      printf("Enter message for client:");
      int n = 0;
      ::bzero(buff_main, sizeof(buff_main));
      while ((buff_main[n++] = getchar()) != '\n')
        ;
      // send message to client
      child_socket.send(buff_main);
    }*/
    chat(child_socket);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
    // fprintf(stderr, e.what());
  }

  printf("All done!\n");
  return 0;
}
