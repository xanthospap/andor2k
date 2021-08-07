#ifndef __ANDOR2K_DAEMON_SEVICE_HPP__
#define __ANDOR2K_DAEMON_SEVICE_HPP__

#include "andor2k.hpp"

/* buffer size for communication (between sockets) */
constexpr int SOCKET_BUFFER_SIZE = 1024;

int resolve_image_parameters(const char *command,  AndorParameters &params) noexcept;

#endif
