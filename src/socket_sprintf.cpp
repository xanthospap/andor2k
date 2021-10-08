#include "andor2kd.hpp"
#include <cstdarg>
#include <cstring>

int socket_sprintf(const andor2k::Socket &socket, char *buffer, const char *fmt,
                   ...) noexcept {
  va_list va;
  va_start(va, fmt);
  std::memset(buffer, 0, MAX_SOCKET_BUFFER_SIZE);
  int end = std::vsprintf(buffer, fmt, va);
  va_end(va);
  end += std::sprintf(buffer + end, ";time:");
  date_str(buffer + end);
  return socket.send(buffer);
}
