#include "andor_time_utils.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using andor2k::ServerSocket;
using andor2k::Socket;

/* responses to client should of the type:
 * TYPE:INFO;
 * when command is done, always send the response:
 * DONE;RETURN_VALUE
 */

char buff_main[1024];
char buff_stat[1024];
int status_th_error;
int doing_work = 0;
int sabort = 0;
int sintrp = 0;

void shutdown(int status) noexcept {
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "command:shutdown;status:closing down deamon");
  printf("shutdown() called\n");
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "done;0");
  std::exit(status);
}

void set_abort(int signal) noexcept {
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "command:abort;status:aborting current work");
  printf("---> Signal caught: %d! setting abort\n", signal);
  // shutdown(signal);
  sabort = 1;
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "done;0");
}

void interrupt(int signal) noexcept {
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "command:abort;status:aborting current work");
  printf("---> Signal caught: %d! setting interrupt\n", signal);
  sintrp = 1;
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "done;0");
}

void set_temp(const Socket &csock) {
  doing_work = 1;

  printf("setting temperature ...\n");
  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "command:settemp;status:server going to work;time:");
  strfdt<DateTimeFormat::YMDHMS>(std::chrono::system_clock::now(),
                                 buff_stat + 47);
  csock.send(buff_stat);

  for (int i = 0; i < 4; i++) {
    std::this_thread::sleep_for(2200ms);
    printf("\tworking ... for function: %s\n", __func__);

    std::memset(buff_stat, 0, 1024);
    sprintf(buff_stat,
            "command:settemp;temp:%+d;status:server doing work (%d/%d);time:",
            -i * 5 + 10, i, 7);
    int end = std::strlen(buff_stat);
    strfdt<DateTimeFormat::YMDHMS>(std::chrono::system_clock::now(),
                                   buff_stat + end);
    if (csock.send(buff_stat) < 0) {
      printf("------ ERROR failed to send message to client --\n");
    }
  }
  doing_work = 0;

  std::memset(buff_stat, 0, 1024);
  sprintf(buff_stat, "done;%d;error:0", 0);
  if (csock.send(buff_stat) < 0) {
    printf("------ ERROR failed to send message to client --\n");
  }
  printf("Server work done!\n");
  return;
}

void do_work(const Socket &csock) {
  doing_work = 1;
  int nimages = 4;

  printf("server taking image ....\n");

  std::memset(buff_stat, 0, 1024);
  std::strcpy(buff_stat, "command:image;status:server going to work;time:");
  strfdt<DateTimeFormat::YMDHMS>(std::chrono::system_clock::now(),
                                 buff_stat + 47);
  csock.send(buff_stat);

  for (int img = 1; img <= nimages; img++) {
    for (int i = 0; i < 5; ++i) {
      std::this_thread::sleep_for(1500ms);

      std::memset(buff_stat, 0, 1024);
      sprintf(buff_stat,
              "command:image;image:%d/%d;progperc:%d;status:server doing work "
              "(%d/%d);time:",
              img, nimages, (i + 1) * 20, i, 7);
      int end = std::strlen(buff_stat);
      strfdt<DateTimeFormat::YMDHMS>(std::chrono::system_clock::now(),
                                     buff_stat + end);
      if (csock.send(buff_stat) < 0) {
        printf("------ ERROR failed to send message to client --\n");
      }
    }
  }
  doing_work = 0;

  std::memset(buff_stat, 0, 1024);
  sprintf(buff_stat, "done;%d;error:0", 0);
  if (csock.send(buff_stat) < 0) {
    printf("------ ERROR failed to send message to client --\n");
  }
  printf("Server work done!\n");
  return;
}

void chat(const Socket &socket) { /* MAIN CHAT */
  for (;;) {

    ::bzero(buff_main, sizeof(buff_main));
    if (socket.recv(buff_main, 1024) <= 0) {
      // client probably ended connection
      return;
    }

    // print client message
    printf("Got string from client: [%s]\n", buff_main);
    if (!std::strncmp("shutdown", buff_main, 8)) {
      printf("-->ShutDown instruction caught at main socket\n");
      sabort = 1;
      break;
    }

    // do some work
    if (!std::strncmp(buff_main, "image", 5)) {
      do_work(socket);
    } if (!std::strncmp(buff_main, "settemp", 7)) {
      set_temp(socket);
    } else {
      printf("unrecognized command: [%s]; valid commands are image and settemp\n", buff_main);
      std::strcpy(buff_main, "Invalid command");
      socket.send(buff_main);
    }

  }
  printf("--> ending main chat and returning\n");
  return;
}

int main() {
  int status;
  sabort = 0;

  // Set signals for interrupts
  signal(SIGHUP, set_abort);
  signal(SIGINT, set_abort);
  signal(SIGQUIT, interrupt);
  signal(SIGTERM, interrupt);
  
  ServerSocket server_sock(8080);
  int child_socket_fd;
  
  while (!sabort) {
    try {

      Socket child_socket = server_sock.accept(status);
      child_socket_fd = child_socket.sockid();
      if (status < 0) {
        fprintf(stderr, "[ERROR] Failed to create child socket!\n");
        return 1;
      }

      printf("Main Server Socket created; client atached and ready!\n");
      chat(child_socket);
      printf("Finsished chating with socket %d\n", child_socket_fd);
      close(child_socket_fd);
      shutdown(child_socket_fd, SHUT_RDWR);

    } catch (std::exception &e) {
      fprintf(stderr, "[ERROR] Exception caught/closing socket\n");
      close(child_socket_fd);
      shutdown(child_socket_fd, SHUT_RDWR);
    }
  }

  printf("Exiting daemon\n");
  printf("All done!\n");
  return 0;
}
