#include "andor2k.hpp"
#include "aristarchos.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>
#include <unistd.h>

int main() {
  
  std::vector<FitsHeader> headers;
  
  int error = get_aristarchos_headers(3, headers);
  
  return error;
}
