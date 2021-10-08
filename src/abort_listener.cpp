#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "cpp_socket.hpp"
#include <condition_variable>
#include <cstring>
#include <mutex>

using andor2k::Socket;

extern std::mutex g_mtx_abort;
extern int abort_set;
extern int abort_socket_fd;
extern std::condition_variable cv;

void abort_listener(int port_no) noexcept {

  char buf[64];
  int sock_status;
  abort_set = 0;

  // do not lock yet!
  std::unique_lock<std::mutex> lk(g_mtx_abort, std::defer_lock);

  // in case this throws, and no socket is created, set the abort_socket_fd to
  // a negative number and call cv.notify_one(). There may be other threads
  // waiting to be notified about the socket. See the catch block below
  try {
    // lock untill we have created a socket
    lk.lock();

    // open a socket to listen to; note that this may throw, in which case
    // we should unlock
    andor2k::ServerSocket server_sock(port_no);
    printf(">> opened socket at port %d to listen for abort ...\n", port_no);

    // set the abort_socket_fd to the newly created socket's fd, so that other
    // fuctions can close it. Notify the other threads that we have created the
    abort_socket_fd = server_sock.sockid();
    lk.unlock();
    cv.notify_one();
    printf(">> abort_socket_fd set, can now close socket outside thread (%d)\n",
           server_sock.sockid());

    // creating hearing child socket (if anyone wants to ever connect ...)
    andor2k::Socket child_socket = server_sock.accept(sock_status);
    if (sock_status < 0) {
      fprintf(stderr, ">> Failed to create child socket ... exiting\n");
      return;
    }

    printf(">> socket request accepted! someone is talking!\n");

    // while (true) {
    std::memset(buf, 0, 64);
    int bytes = child_socket.recv(buf, 64);

    // client closed connection
    if (bytes <= 0) {
      printf(">> closing abort thread/socket connection\n");
      return;
    }

    // we have received something! interpreting as abort signal
    printf(">> abort signal caught from client!\n");
    abort_set = 1;
    unsigned int error = CancelWait();
    printf(">> CancelWait() returned %d (success?%d)\n", error,
           error == DRV_SUCCESS);
    return;
    //}

  } catch (std::exception &e) {
    printf(">>> closing abort thread/socket\n");
    if (!lk.owns_lock())
      lk.lock();
    abort_socket_fd = -1;
    lk.unlock();
    cv.notify_one();
    return;
  }

  return;
}
