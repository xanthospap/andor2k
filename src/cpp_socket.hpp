#ifndef __HELMOS_ANDOR2K_SOCKETS_HPP__
#define __HELMOS_ANDOR2K_SOCKETS_HPP__

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef SOCKET_LOGGER
#include <fstream>
#endif

namespace andor2k {

#ifdef SOCKET_LOGGER
class SocketLogger {
private:
  std::ofstream m_logger;
  char tm_buf[128];

public:
  SocketLogger(const char *fn);
  ~SocketLogger() noexcept;
  SocketLogger(const SocketLogger &) = delete;
  SocketLogger &operator=(const SocketLogger &) = delete;
  void print_msg(int sockid, const char *msg, int info_type);
  void print_msg(int sockid, std::string msg, int info_type) {
    return print_msg(sockid, msg.c_str(), info_type);
  }
}; // SocketLogger
#endif

class Socket {
private:
  int m_sockid;          ///< the socket's file descriptor
  sockaddr_in m_address; ///< the (socket's) address
#ifdef SOCKET_LOGGER
  SocketLogger *m_logger;
#endif

public:
  /// @brief Create a new socket (assigns m_sockid). On failure m_sockid will
  ///        be set to -1. Any other value means the socket is created.
  /// @warning The constructor mat fail, so before further accessing the
  ///          socket, make sure that its m_sockid is not -1
#ifdef SOCKET_LOGGER
  Socket(SocketLogger *logger) noexcept;
#else
  Socket() noexcept;
#endif

  /// @brief Constructor given a socket file descriptor and its address
  /// @param[in] sockid The socket's file descriptor
  /// @param[in] addr   The socket's address
#ifdef SOCKET_LOGGER
  Socket(int sockid, sockaddr_in addr, SocketLogger *logger) noexcept;
#else
  Socket(int sockid, sockaddr_in addr) noexcept;
#endif

  /// @brief Copying is not allowed
  Socket(const Socket &) = delete;

  /// @brief Copy via equality operator not allowed
  Socket &operator=(const Socket &) = delete;

  /// @brief Movying is allowed
  Socket(Socket &&) = default;

  /// @brief Destructor; closes the socket's file descriptor
  ~Socket() noexcept;

  int socket_close() { return close(m_sockid); }

  /// @brief Get the instances socket id, aka the socket file descriptor
  int sockid() const noexcept { return m_sockid; }

  /// @brief Assign the socket's address, aka will do the following
  /// * set sin_family to AF_INET
  /// * set sin_port to port (actually htons(port))
  /// * set sin_addr.s_addr to INADDR_ANY if ip=nullptr else set it to
  ///   inet_addr(ip)
  /// The aforementioned value will be assigned to the instance's address
  /// (m_address).
  /// @note The function only accepts actual ip's NOT hostnames. The only
  ///       exception is 'localhost' which will be translated to '127.0.0.1'
  /// @param[in] port The port number
  /// @param[in] ip The ip (NOT hostname) to be assigned
  void set_sock_addr(int port, const char *ip = nullptr) noexcept;

  /// @brief Get MAXHOSTNAME
  constexpr int maxhostname() const noexcept { return 200; }

  /// @brief get MAX_CONNECTIONS
  constexpr int maxconnections() const noexcept { return 5; }

  /// @brief set a socket option
  /// @return Upon successful completion, the value 0 is returned; otherwise
  ///         the value -1 is returned and the global variable errno is set to
  ///         indicate the error.
  /// TODO not tested!
  template <typename T>
  int set_option(int level, int optname, T optvalue) noexcept {
    T val{optvalue};
    return setsockopt(m_sockid, level, optname, &val, sizeof(T));
  }

  /// @brief Send message on this socket.
  /// @param[in] msg The message as a null reminated C-string to send
  /// @param[in] flag Send flags (normally 0)
  /// @return On success, the number of characters sent. On error, -1 is
  ///         returned, and errno is set appropriately.
  /// @note The send() call may be used only when the socket is in a connected
  ///       state.
  ///       When the message does not fit into the send buffer of the socket,
  ///       send() normally blocks, unless the socket has been placed in
  ///       nonblocking I/O mode. In nonblocking mode it would fail with the
  ///       error EAGAIN or EWOULDBLOCK in this case.
  ///       The size of the message is computed using a call to strlen().
  /// @see https://linux.die.net/man/2/send
  int send(const char *msg, int flag = 0) const noexcept;

  /// @brief Receive a message from this socket.
  /// @param[in] buffer A (char) buffer to assign the read in bytes (actually
  ///                   a null reminated C-string)
  /// @param[in] buf_sz The buffer size
  /// @param[in] flag Receive flags (normally 0)
  /// @return Returns the number of bytes received, or -1 if an error occurred.
  ///         In the event of an error, errno is set to indicate the error.
  /// @note The recv() call is normally used only on a connected socket.
  ///       If a message is too long to fit in the supplied buffer, excess
  ///       bytes may be discarded depending on the type of socket the message
  ///       is received from.
  ///       If no messages are available at the socket, the receive calls wait
  ///       for a message to arrive, unless the socket is nonblocking, in which
  ///       case the value -1 is returned and the external variable errno is
  ///       set to EAGAIN or EWOULDBLOCK. The receive calls normally return any
  ///       data available, up to the requested amount, rather than waiting for
  ///       receipt of the full amount requested.
  /// @see https://linux.die.net/man/2/recv
  int recv(char *buffer, std::size_t buf_sz, int flag = 0) const noexcept;

  /// @bind Bind a name to a socket (for server sockets).
  /// When a socket is created with socket(), it exists in a name space
  /// (address family) but has no address assigned to it. bind() assigns the
  /// address specified by (the instance's m_addr) to the socket. Theat is,
  /// the function will first call setSockaddr(port) to assign the port given
  /// to the instance's address and then bind the socket.
  /// Traditionally, this operation is called "assigning a name to a socket".
  /// @param[in] port The port number.
  /// @return On success, zero is returned. On error, -1 is returned, and
  ///         errno is set appropriately.
  /// @note The function will first call setSockaddr(port) to set the instance
  ///       m_addr variable.
  ///       It is normally necessary to assign a local address using bind()
  ///       before a SOCK_STREAM socket may receive connections.
  /// @see https://linux.die.net/man/2/bind
  int bind(int port) noexcept;

  /// @brief Listen for connections on a socket (for server sockets).
  /// listen() marks the socket referred to by sockfd as a passive socket, that
  /// is, as a socket that will be used to accept incoming connection requests
  /// using accept().
  /// @param[in] maxcon This argument defines the maximum length to which the
  ///                   queue of pending connections for this socket may grow.
  ///                   If a connection request arrives when the queue is full,
  ///                   the client may receive an error with an indication of
  ///                   ECONNREFUSED or, if the underlying protocol supports
  ///                   retransmission, the request may be ignored so that a
  ///                   later reattempt at connection succeeds.
  ///                   If maxcon<0 maxcon will be set to maxconnections().
  /// @return On success, zero is returned.  On error, -1 is returned, and
  ///         errno is set appropriately.
  /// @see https://linux.die.net/man/2/listen
  int listen(int maxcon = -1) const noexcept;

  /// @brief Accept a connection on a socket (for server sockets).
  /// The accept() call is used with connection-based socket types
  /// (SOCK_STREAM, SOCK_SEQPACKET). It extracts the first connection request
  /// on the queue of pending connections for the listening socket (aka the
  /// calling socket), creates a new connected socket, and returns this new
  /// socket. The newly created socket is not in the listening state. The
  /// original socket (aka the calling socket) is unaffected by this call.
  /// @param[in] status The returned function status after the accept call;
  ///         On success, the call returns a file descriptor for the accepted
  ///         socket (a nonnegative integer). On error, -1 is returned, errno
  ///         is set appropriately, and addrlen is left unchanged.
  /// @return The newly created socket (connected)
  /// @see https://linux.die.net/man/2/accept
  Socket accept(int &status) noexcept;

  /// @brief Initiate a connection on a socket (for client sockets).
  /// The connect() call connects the socket to the address specified by the
  /// input parameters.
  /// If the socket is of type SOCK_STREAM or SOCK_SEQPACKET, this call
  /// attempts to make a connection to the socket that is bound to the address
  /// specified by addr.
  /// Generally, connection-based protocol sockets may successfully connect()
  /// only once; connectionless protocol sockets may use connect() multiple
  /// times to change their association.
  /// The function will first call setSockaddr(port, ip) to set the instance's
  /// m_addr to the given address (aka set ip and port). It will then call
  /// connect() to connect the calling socket to the m_addr.
  /// @return Upon successful completion, the function shall return 0;
  ///         otherwise, -1 shall be returned and errno set to indicate the
  ///         error.
  /// @see https://linux.die.net/man/2/connect
  int connect(const char *ip, int port) noexcept;

  /// @return On error, -1 is returned, and errno is set appropriately.
  int set_non_blocking() noexcept;

}; // Socket

class ClientSocket {
public:
  /// @details Constructor for client sockets. The function will perform the
  ///          following:
  ///          * create a new socket
  ///          * connect to the given address
  ///          If the constructor fails in any of the above steps, it will
  ///          throw. If it does not fail, the socket is ready for send/receive
  ///          operations.
  /// @param[in] ip The ip to connect the socket to (note that this must be a
  ///            valid ip and NOT a hostname; the only hostname allowed is
  ///           "localhost" which will be translated to "127.0.0.1".
  /// @param[in] port The port to connect to.
  ClientSocket(const char *ip, int port
#ifdef SOCKET_LOGGER
               ,
               SocketLogger *logger
#endif
  );

  /// @brief Send data
  int send(const char *msg, int flag = 0) const noexcept {
    return m_socket.send(msg, flag);
  }

  /// @brief Receive data
  int recv(char *buffer, std::size_t buf_sz) const noexcept {
    return m_socket.recv(buffer, buf_sz);
  }

  int close_socket() noexcept { return m_socket.socket_close(); }

  auto sockid() noexcept { return m_socket.sockid(); }

private:
  Socket m_socket;
}; // ClientSocket

class ServerSocket {
public:
  /// @details Constructor for server sockets. The function will perform the
  ///          following:
  ///          * create a new socket
  ///          * set the port (aka call setSockaddr(port))
  ///          * bind the socket
  ///          * listen for connections on the socket
  ///          If the constructor fails in any of the above steps, it will
  ///          throw. If it does not fail, the socket is ready to create new
  ///          sockets via accepting connections.
  /// @param[in] port The port to connect to.
  ServerSocket(int port
#ifdef SOCKET_LOGGER
               ,
               SocketLogger *logger
#endif
  );

  /// @brief send data
  int send(const char *msg, int flag = 0) const noexcept {
    return m_socket.send(msg, flag);
  }

  /// @brief receive data
  int recv(char *buffer, std::size_t buf_sz) const noexcept {
    return m_socket.recv(buffer, buf_sz);
  }

  /// @brief Accept a connection on a socket
  Socket accept(int &status) noexcept { return m_socket.accept(status); }

  auto sockid() noexcept { return m_socket.sockid(); }

private:
  Socket m_socket;
}; // ServerSocket
} // namespace andor2k
#endif
