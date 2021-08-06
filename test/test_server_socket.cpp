#include "cpp_socket.hpp"
#include <cstring>
#include <iostream>
#include <thread>
#include <pthread.h>
#include <chrono>
#include <unistd.h>
#include <csignal>

using namespace std::chrono_literals;
using andor2k::ServerSocket;
using andor2k::Socket;

char buff_main[1024];
char buff_stat[1024];
int status_th_error;
int doing_work = 0;
int sabort = 0;
int sintrp = 0;

void shutdown(int status) noexcept {
  printf("shutdown() called\n");
  std::exit(status);
}

void set_abort(int signal) noexcept {
  printf("---> Signal caught: %d! setting abort\n", signal);
  // shutdown(signal);
  sabort = 1;
}

void interrupt(int signal) noexcept {
  printf("---> Signal caught: %d! setting interrupt\n", signal);
  sintrp = 1;
}

void do_work(const Socket& csock) {
  doing_work = 1;
  char sbuf[1024];
  
  printf("server doing work ....\n");
  std::strcpy(sbuf, "server going to work");
  csock.send(sbuf);

  for (int i=0; i<3; ++i) {
    std::this_thread::sleep_for(4000ms);
    printf("\tworking ... for function: %s\n", __func__);

    std::memset(sbuf, 0, 1024);
    std::sprintf(sbuf, "Server doing work (%d/%d)", i, 7);
    if (csock.send(sbuf)<0) {
      printf("------ ERROR failed to send message to client --\n");
    }

    if (sabort) {
      printf("\tstop working now! sabort set!\n");
      doing_work = 0;
      return;
    }
    if (sintrp) {
      printf("\tstop working now! interupt set!\n");
      doing_work = 0;
      sintrp = 0;
      return;
    }
  }
  doing_work = 0;

  std::memset(sbuf, 0, 1024);
  sprintf(sbuf, "done %d", 0);
  if (csock.send(sbuf)<0) {
    printf("------ ERROR failed to send message to client --\n");
  }
  printf("Server work done!\n");
  return;
}

void chat(const Socket &socket) { /* MAIN CHAT */
  for (;;) {
    ::bzero(buff_main, sizeof(buff_main));
    // read message from client into buff_main
    socket.recv(buff_main, 1024);
    
    // print client message
    printf("Got string from client: %s\n", buff_main);
    if (!std::strncmp("shutdown", buff_main, 8)) {
      printf("-->ShutDown instruction caught at main socket\n");
      sabort = 1;
      break;
    }

    // do some work
    if (!std::strncmp(buff_main, "image", 5))
      do_work(socket);

    if (sabort) {
      printf("abort set; leaving main chat!\n");
      sabort = 1;
      break;
    }

    /* get message for client
    printf("Enter message for client:");
    int n = 0;
    ::bzero(buff_main, sizeof(buff_main));
    while ((buff_main[n++] = getchar()) != '\n')
      ;
    
    // send message to client
    socket.send(buff_main);*/
  }
  printf("--> ending main chat and returning\n");
  return;
}

void *statusSockFunc(void *ptr) noexcept { /* PARALLEL CHAT */
  int error = 0;
  pid_t ipid = getpid();
  status_th_error = 0;

  ptr = nullptr;
  if (ptr) printf("what??");
  
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

      if (sabort) {
        printf("abort set; leaving chat -- 2!\n");
        return (void*)(&status_th_error);
      }
      
      // do not print client message
      // printf("Got string from client: %s (status socket)", buff_stat);
      if (!std::strncmp("shutdown", buff_stat, 8)) break;

      if (!std::strncmp("abort", buff_stat, 5)) {
        sabort = 1;
        break;
      }
      
      if (!std::strncmp("interrupt", buff_stat, 5)) {
        sintrp = 1;
        break;
      }
      
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
      
      if (sabort) {
        printf("abort set; leaving chat -- 2!\n");
        break;
      }
      
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

  // Set signals for interrupts                                                 
  signal(SIGHUP, set_abort);
  signal(SIGINT, set_abort);
  signal(SIGQUIT, interrupt);
  signal(SIGTERM, interrupt);

  // create a socket dedicated to listen for status
  // pthread_t status_thread;
  // int thread_error = pthread_create(&status_thread, NULL, statusSockFunc, NULL);

  try {
    ServerSocket server_sock(8080);

    Socket child_socket = server_sock.accept(status);
    if (status < 0) {
      fprintf(stderr, "[ERROR] Failed to create child socket!\n");
      return 1;
    }

    printf("Main Server Socket created; client atached and ready!\n");
    chat(child_socket);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR] Exception caught!\n");
  }

  printf("Exiting daemon\n");
  printf("All done!\n");
  return 0;
}
