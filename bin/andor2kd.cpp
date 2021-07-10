#include "cpp_socket.hpp"
#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <ctime>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>
#include <csignal>

using andor2k::ServerSocket;
using andor2k::Socket;
using namespace std::chrono_literals;

constexpr int SOCKET_PORT = 8080;
char fits_file[256] = {'\0'};
char now_str[32] = {'\0'}; // YYYY-MM-DD HH:MM:SS
AndorParameters params;

const char *date_str() noexcept {
  // get current datetime
  std::time_t now = std::time(nullptr);
  // convert now to Local Time.
  std::tm *now_loct = ::localtime(&now);
  // should write 8 chars (4+2+2) not including the null terminating char
  std::strftime(now_str, 32, "%D %T", now_loct);
  return now_str;
}

void kill_daemon(int sig) noexcept {
    printf("[DEBUG][%s] Caught signal (#%d); shutting down daemon\n", date_str(), sig);
    // system_shutdown(&params);
    printf("[DEBUG][%s] Goodbye!\n", date_str());
    exit(sig);
}

int main() {
    int sock_status;

    // register signal SIGINT and signal handler  
    signal(SIGINT, kill_daemon);  
    
    try {
        ServerSocket server_sock(SOCKET_PORT);

        /* report daemon initialization */
        printf("[DEBUG][%s] Initializing ANDOR2K daemon service\n", date_str());
        printf("[DEBUG][%s] Listening on port %d\n", date_str(), SOCKET_PORT);
        printf("[DEBUG][%s] Service is up and running ... waiting for input\n", date_str());

        /* creating hearing child socket */
        Socket child_socket = server_sock.accept(sock_status);
        if (sock_status < 0) {
            fprintf(stderr, "[FATAL][%s] Failed to create child socket ... exiting\n", date_str());
            return 1;
        }

    } catch (std::exception& e) {
        fprintf(stderr, "[ERROR][%s] Failed creating deamon\n", date_str());
        // fprintf(stderr, "[ERROR] what(): %s". e.what());
        fprintf(stderr, "[FATAL][%s] ... exiting\n", date_str());
        return 1;
    }
}