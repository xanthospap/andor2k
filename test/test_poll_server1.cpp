#include "cpp_socket.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using andor2k::ServerSocket;
using andor2k::Socket;

#define SERVER_SOCKET_PORT 8080
#define MAX_BYTES_IN_MESSAGE 256
#define MAX_ALLOWED_CONNECTIONS 5

char buff_main[MAX_BYTES_IN_MESSAGE];
char buff_stat[MAX_BYTES_IN_MESSAGE];

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int &fd_count) noexcept {
  (*pfds)[fd_count].fd = newfd;
  (*pfds)[fd_count].events = POLLIN; // Check ready-to-read

  fd_count++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int &fd_count,
                   std::vector<Socket *> &sockets) noexcept {
  // Copy the one from the end over this one
  pfds[i] = pfds[fd_count - 1];
  fd_count--;
  // erase from vector
  assert(pfds[i].fd == sockets[i]->sockid());
  sockets.erase(sockets.begin() + i);
}

// receiver bytes from socket with given id
int receive_from(int fd, const std::vector<Socket *> &sockets) noexcept {
  auto sock = std::find_if(sockets.begin(), sockets.end(),
                           [fd](const Socket *s) { return s->sockid() == fd; });
  if (sock == sockets.end()) {
    fprintf(stderr, "<%s> Failed to match fd to socket!\n", __func__);
    return -100;
  }
  std::memset(buff_main, '\0', MAX_BYTES_IN_MESSAGE);
  return (*sock)->recv(buff_main, MAX_BYTES_IN_MESSAGE);
}

int send_to(int fd, const std::vector<Socket *> &sockets) noexcept {
  auto sock = std::find_if(sockets.begin(), sockets.end(),
                           [fd](const Socket *s) { return s->sockid() == fd; });
  if (sock == sockets.end()) {
    fprintf(stderr, "<%s> Failed to match fd to socket!\n", __func__);
    return -100;
  }
  printf("<%s> Sending some data from socket %d\n", __func__, fd);
  std::strcpy(buff_stat, "here is an answer with 0");
  return (*sock)->send(buff_stat);
}

int close_socket(int fd, std::vector<Socket *> &sockets) noexcept {
  auto sock = std::find_if(sockets.begin(), sockets.end(),
                           [fd](const Socket *s) { return s->sockid() == fd; });
  if (sock == sockets.end()) {
    fprintf(stderr, "<%s> Failed to match fd to socket!\n", __func__);
    return -100;
  }
  return (*sock)->socket_close();
}

int main() {
  // int listener_id;                    // Listening socket descriptor
  int newfd; // Newly accept()ed socket descriptor
  // struct sockaddr_storage remoteaddr; // Client address
  // socklen_t addrlen;
  int fd_count = 0;
  struct pollfd *pfds = static_cast<struct pollfd *>(
      malloc(sizeof *pfds * MAX_ALLOWED_CONNECTIONS));

  std::vector<Socket *> socket_vec;
  socket_vec.reserve(MAX_ALLOWED_CONNECTIONS + 1);

  // Set up and get a listening socket
  ServerSocket listener(SERVER_SOCKET_PORT);
  int listener_id = listener.sockid();

  // Add the listener to set
  pfds[0].fd = listener_id;
  pfds[0].events = POLLIN; // Report ready to read on incoming connection
  // socket_vec.push_back(&listener);
  fd_count = 1; // For the listener

  // Main loop
  for (;;) {
    int poll_count = poll(pfds, fd_count, -1);

    if (poll_count == -1) {
      perror("poll");
      exit(1);
    }

    // Run through the existing connections looking for data to read
    for (int i = 0; i < fd_count; i++) {

      // Check if someone's ready to read
      if (pfds[i].revents & POLLIN) { // We got one!!

        if (pfds[i].fd == listener_id) {
          // If listener is ready to read, handle new connection

          if (fd_count == MAX_ALLOWED_CONNECTIONS) {
            fprintf(stderr,
                    "<%s> Max allowed connections reached! Not allowing new "
                    "connection request\n",
                    __func__);
            return 50;
          }

          int status;
          Socket child_socket = listener.accept(status);
          newfd = child_socket.sockid();

          if (status < 0 || newfd < 0) {
            fprintf(stderr, "<%s> Failed to bind/accept new socket!\n",
                    __func__);
          } else {
            add_to_pfds(&pfds, newfd, fd_count);
            socket_vec.push_back(&child_socket);

            printf("<%s> New incoming connection on socket %d\n", __func__,
                   newfd);
            printf("<%s> Current live sockets: ", __func__);
            for (int f = 0; f < fd_count; f++)
              printf("%d ", pfds[f].fd);
            printf("\n");
          }
        } else {
          // If not the listener, we're just a regular client
          printf("<%s> incoming message from socket %d, reading ...\n",
                 __func__, pfds[i].fd);
          int nbytes = receive_from(pfds[i].fd, socket_vec);

          int sender_fd = pfds[i].fd;

          if (nbytes <= 0) {
            // Got error or connection closed by client
            if (nbytes == 0) {
              // Connection closed
              printf("<%s> socket %d hung up\n", __func__, sender_fd);
            } else {
              fprintf(stderr, "<%s> Failed receiving message from socket %d\n",
                      __func__, sender_fd);
            }
            close_socket(pfds[i].fd, socket_vec); // Bye!
            del_from_pfds(pfds, i, fd_count, socket_vec);

          } else {
            // We got some good data from a client

            for (int j = 0; j < fd_count; j++) {
              // Send to everyone!
              int dest_fd = pfds[j].fd;

              // Except the listener and ourselves
              if (dest_fd != listener_id && dest_fd != sender_fd) {
                printf("<%s> sending message from socket %d\n", __func__,
                       dest_fd);
                if (send_to(dest_fd, socket_vec) < 0) {
                  fprintf(stderr,
                          "<%s> Failed to send message from socket %d\n",
                          __func__, dest_fd);
                }
              }
            }
          }
        } // END handle data from client
      }   // END got ready-to-read from poll()
    }     // END looping through file descriptors
  }       // END for(;;)--and you thought it would never end!

  return 0;
}