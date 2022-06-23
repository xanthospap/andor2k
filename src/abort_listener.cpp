#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "cpp_socket.hpp"
#include <condition_variable>
#include <cstring>
#include <mutex>

extern std::mutex g_mtx_abort;
extern int abort_set;
extern int abort_socket_fd;
extern std::condition_variable cv;

/// This function will try to open a new listening socket on port port_no; if
/// successeful, the (global) variable abort_socket_fd will be set to the new
/// socket's file descriptor (fd) so that other function can see it (and close
/// it if needed).
/// When called, it will try to get a lock of the (global) g_mtx_abort mutex;
/// once the lock is successeful, it will create the socket, set the
/// abort_socket_fd (global) variable the socket's fd and unlock the mutex.
/// Then it will notify any other waiting (for the g_mtx_abort) functions that
/// the job is done, and the socket will be in a state ready to accept any
/// incoming connections.
/// Note that the notification will be performed using the (global) cv condition
/// variable.
/// The newly created socket will wait for any incoming connection; if a message
/// is received, it will be interpreted as an abort signal, hence:
/// 1. the global variable abort_set will be set to 1, and
/// 2. the CancelWait() funcation will be called (to cancel any call to
/// WaitForAcquisition() in any other running thread)
/// @param[in] port_no Port number to listen to for incoming connections
void abort_listener(int port_no) noexcept {

  char buf[64];
#ifdef DEBUG
  char dbuf[64]; // for reporting datetime
#endif
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
    // we should unlock (see the catch block)
    andor2k::ServerSocket server_sock(port_no);
#ifdef DEBUG
    printf("[DEBUG][%s] opened listening socket localhost:%d to listen for "
           "abort ...\n",
           date_str(dbuf), port_no);
#endif

    // set the abort_socket_fd to the newly created socket's fd, so that other
    // fuctions can close it. Notify the other threads that we have created the
    // socket.
    abort_socket_fd = server_sock.sockid();
    lk.unlock();
    cv.notify_one();
#ifdef DEBUG
    printf("[DEBUG][%s] abort_socket_fd set, can now close socket outside "
           "thread (%d)\n",
           date_str(dbuf), server_sock.sockid());
#endif

    // accept/bind any incommming connection (if ever someone tries to connect)
    andor2k::Socket child_socket = server_sock.accept(sock_status);
    if (sock_status < 0) {
#ifdef DEBUG
      fprintf(stderr,
              "[ERROR][%s] Failed to accept/bind incoming connection on "
              "localhost:%d (fd=%d); exiting %s (traceback: %s)\n",
              date_str(dbuf), port_no, abort_socket_fd, __func__, __func__);
#endif
      return;
    }

#ifdef DEBUG
    printf("[DEBUG][%s] socket request accepted! someone is talking to "
           "localhost:%d!\n",
           date_str(dbuf), port_no);
#endif

    // we are only going to accept one incomming message; then exit
    std::memset(buf, 0, 64);
    int bytes = child_socket.recv(buf, 64);

    // client closed connection
    if (bytes <= 0) {
#ifdef DEBUG
      printf("[DEBUG][%s] closing abort socket connection localhost:%d\n",
             date_str(dbuf), port_no);
#endif
      return;
    }

    // we have received something! interpreting as abort signal
#ifdef DEBUG
    printf("[DEBUG][%s] abort signal caught from client at localhost:%d!\n",
           date_str(dbuf), port_no);
#endif
    abort_set = 1;
    unsigned int error = CancelWait();
#ifdef DEBUG
    printf("[DEBUG][%s] CancelWait() called, returned %d (success?%d)\n",
           date_str(dbuf), error, error == DRV_SUCCESS);
#endif
    return;

  } catch (std::exception &e) {
#ifdef DEBUG
    printf("[DEBUG][%s] closing abort thread/socket on localhost:%d\n",
           date_str(dbuf), port_no);
#endif
    // set the socket fd to an error state and notify that everyting is done
    if (!lk.owns_lock())
      lk.lock();
    abort_socket_fd = -1;
    lk.unlock();
    cv.notify_one();
    return;
  }

  return;
}
