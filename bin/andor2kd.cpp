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
#include <cstring>

using andor2k::ServerSocket;
using andor2k::Socket;
using namespace std::chrono_literals;

/* ANDOR2K REALTED CONSTANTS */
constexpr int ANDOR_MIN_TEMP = -120;
constexpr int ANDOR_MAX_TEMP = 10;

/* buffers and constants */
constexpr int SOCKET_PORT = 8080;
char fits_file[256] = {'\0'};
char now_str[32] = {'\0'}; // YYYY-MM-DD HH:MM:SS
char buffer[1024]; // used by sockets to communicate with client
AndorParameters params;

const char *date_str() noexcept {
  std::time_t now = std::time(nullptr);
  std::tm *now_loct = ::localtime(&now);
  std::strftime(now_str, 32, "%D %T", now_loct);
  return now_str;
}

void kill_daemon(int sig) noexcept {
    printf("[DEBUG][%s] Caught signal (#%d); shutting down daemon\n", date_str(), sig);
    // system_shutdown(&params);
    printf("[DEBUG][%s] Goodbye!\n", date_str());
    exit(sig);
}

int set_temperature() noexcept {
    if ( std::strncmp(buffer, "settemp", 7) )
        return 1;
    // we expect that after the temperatues, we have a valid int
    char* end;
    int target_temp = std::strtol(buffer+7, &end, 10);
    if (end==buffer+7 || errno) {
        errno = 0;
        fprintf(stderr, "[ERROR][%s] Failed to resolve target temperature in command \"%s\"\n", date_str(), buffer);
        fprintf(stderr, "[ERROR][%s] Skippig command\n", now_str, buffer);
        return 1;
    }
    if (target_temp<ANDOR_MIN_TEMP || target_temp>ANDOR_MAX_TEMP) {
        fprintf(stderr, "[ERROR][%s] Refusing to set temperature outside limits: [%+3d, %+3d]\n", now_str, ANDOR_MIN_TEMP, ANDOR_MAX_TEMP);
        fprintf(stderr, "[ERROR][%s] Skippig command\n", now_str, buffer);
        return 1;
    }
    // command seems ok .... do it!
    return cool_to_temperature(target_temp);
}

int resolve_command() noexcept {
    if ( !(std::strncmp(buffer, "settemp", 7)) ) {
        return set_temperature();
    } else if ( !(std::strncmp(buffer, "shutdown", 8)) ) {
        return -100;
    } else if ( !(std::strncmp(buffer, "status", 6)) ) {
        return print_status();
    } else {
        fprintf(stderr, "[ERROR][%s] Failed to resolve command: \"%s\"; doing nothing!\n", date_str(), buffer);
        return 1;
    }
}

void chat(const Socket &socket) {
  for (;;) {
    
    // read message from client into buffer
    std::memset(buffer, '\0', sizeof(buffer));
    socket.recv(buffer, 1024);
    
    // print client message
    printf("Got string from client: %s", buffer);
    int answr = resolve_command();
    if (answr == -100) {
        printf("[DEBUG][%s] Received shutdown command; initializing exit sequence\n", date_str());
        break;
    }
    
    // get message for client
    // printf("Enter message for client:");
    // int n = 0;
    // std::memset(buffer, '\0', sizeof(buffer));
    // while ((buffer[n++] = getchar()) != '\n')
    //  ;
    buffer[0] = '0';
    buffer[1] = '\0';
    if (answr) buffer[0] = '1';
    
    // send message to client
    socket.send(buffer);
    
    // if message contains "exit" end chat
    if (std::strncmp("exit", buffer, 4) == 0) {
      printf("Server Exit ...\n");
      break;
    }
  }

  return;
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
        printf("[DEBUG][%s] Waiting for instructions ...\n", date_str());

        /* communicate with client */
        chat(child_socket);

    } catch (std::exception& e) {
        fprintf(stderr, "[ERROR][%s] Failed creating deamon\n", date_str());
        // fprintf(stderr, "[ERROR] what(): %s". e.what());
        fprintf(stderr, "[FATAL][%s] ... exiting\n", date_str());
        return 1;
    }
}