#ifndef __ANDOR2K_DAEMON_SEVICE_HPP__
#define __ANDOR2K_DAEMON_SEVICE_HPP__

#include "andor2k.hpp"
#include "cpp_socket.hpp"

int resolve_image_parameters(const char *command,
                             AndorParameters &params) noexcept;
int socket_sprintf(const andor2k::Socket &socket, char *buffer, const char *fmt,
                   ...) noexcept;
void abort_listener(int port_no) noexcept;

#endif
