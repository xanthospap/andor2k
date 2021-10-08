#include "acquisition_reporter.hpp"
#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include <chrono>
#include <condition_variable>
#include <cppfits.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using andor2k::Socket;

extern std::mutex g_mtx;
extern std::mutex g_mtx_abort;
extern int abort_set;
extern int abort_socket_fd;
extern std::condition_variable cv;

int main() {
  // try doing this 10 times in a row to simulate a realistic scenario
  for (int i = 0; i < 10; i++) {
    printf("-- TRY %d/10 --\n", i + 1);

    // first off, let's create an abort signal listener thread/socket. spawn off
    // the thread and wait till we are notified that we have the socket's fd
    abort_socket_fd = -100;
    std::thread abort_t(abort_listener, SOCKET_PORT + 1);
    std::unique_lock<std::mutex> lk(g_mtx_abort);
    cv.wait(lk, [] { return abort_socket_fd; });
    lk.unlock();

    printf("by now, anyone willing to talk to socket at %d with fd=%d should "
           "be able to\n",
           SOCKET_PORT + 1, abort_socket_fd);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    printf("ok, time has passed, let's close the listening socket\n");

    // int error = close(abort_socket_fd);
    int error = shutdown(abort_socket_fd, 2);
    printf("close returned: %d\n", error);

    abort_t.join();
  }

  return 0;
}
