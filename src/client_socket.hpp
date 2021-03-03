#ifndef __ARISTARCHOS_CLIENT_SOCKET_HPP__
#define __ARISTARCHOS_CLIENT_SOCKETS_HPP__

#include "cpp_sockets.hpp"

namespace aristarchos {
class ClientSocket : private Socket {
public:
  ClientSocket(const char *host, int port) noexcept virtual ~Socket() noexcept;

private:
  int m_file_descr; ///< the socket's file descriptor
};                  // Socket
} // namespace aristarchos
#endif
