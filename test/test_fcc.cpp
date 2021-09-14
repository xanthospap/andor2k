#include "andor2k.hpp"
#include "aristarchos.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using andor2k::ClientSocket;
using andor2k::Socket;

int main() {
  std::vector<FitsHeader> headers;
  int error = get_aristarchos_headers(3, headers);
  return error;
}

/*
#define FCC_IP "195.251.202.253"
#define FCC_PORT 50001

const char *ar_header_command_seq[] = {
    "0003RD;", "0006RE ON;", "0006RE OF;", "0003RS;"};
    //"0006RE ON;", "0006RE OF;", "0002BF"};
constexpr int ar_header_answer_seq[] = {false, true, false, true};
constexpr int ar_header_sleep_seq[] = {8, 0, 2, 0};
constexpr int ar_header_command_seq_size = 4;

bool response_has_error(const char *response) noexcept {
  return *response && (response[4] == '?' || response[5] == '?');
}

int main() {
  char str_buffer_long[ARISTARCHOS_DECODE_BUFFER_SIZE];
  char str_buffer_short[ARISTARCHOS_MAX_SOCKET_BUFFER_SIZE];
  int max_tries = 5;
  int error_occured = 1;
  int count = 0;

  while (count < max_tries && error_occured) {
    error_occured = 0;
    try {
      ++count;
      int error;
      ClientSocket client_socket(FCC_IP, FCC_PORT);
      printf("<%s> client_socket established, seems ok!\n", __func__);

      for (int i = 0; i < ar_header_command_seq_size; i++) {
        std::strcpy(str_buffer_short, ar_header_command_seq[i]);
        
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        int bt_sent = client_socket.send(str_buffer_short);
        if (bt_sent <= 0) {
          fprintf(stderr,
                  "<%s> seems like no bytes tranfered to server; request was: "
                  "[%s]\n",
                  __func__, str_buffer_short);
          fprintf(stderr, "<%s> aborting effort!\n", __func__);
          error_occured = 1;
          break;
        } else {
          printf("<%s> sent command to server [%s]\n", __func__,
                 str_buffer_short);
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        if (ar_header_answer_seq[i]) { // we need a reply
          std::memset(str_buffer_long, '\0',
                      ARISTARCHOS_MAX_SOCKET_BUFFER_SIZE);
          int bt_recv = client_socket.recv(str_buffer_long,
                                           ARISTARCHOS_MAX_SOCKET_BUFFER_SIZE);
          if (bt_recv <= 0) {
            fprintf(
                stderr,
                "<%s> seems like no bytes received server; request was: [%s]\n",
                __func__, str_buffer_short);
          } else {
            printf("<%s> server responded with [%s]\n", __func__,
                   str_buffer_long);
            if (response_has_error(str_buffer_long)) {
              fprintf(stderr,
                      "<%s> seems like the response signaled an error!\n",
                      __func__);
              // fprintf(stderr, "<%s> aborting effort!\n", __func__);
              // error_occured = 1;
              // break;
            }
          }
        } else {
          printf("<%s> not waiting for reply!\n", __func__);
        }// end reply

        std::this_thread::sleep_for(std::chrono::milliseconds(ar_header_sleep_seq[i]*1000L));
      }
      client_socket.close_socket();
    } catch (std::exception &e) {
      fprintf(stderr, "<%s> Failed to open Client Socket for FCC\n", __func__);
      fprintf(stderr, "<%s> Exception: %s\n", __func__, e.what());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  return 51;
}
*/