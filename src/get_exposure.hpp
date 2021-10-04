#include "andor2k.hpp"
#include "andor2kd.hpp"

int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const andor2k::Socket &socket) noexcept;
