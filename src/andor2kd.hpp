#ifndef __ANDOR2K_DAEMON_SEVICE_HPP__
#define __ANDOR2K_DAEMON_SEVICE_HPP__

#include "andor2k.hpp"

int resolve_image_parameters(const char *command,
                             AndorParameters &params) noexcept;

#endif
