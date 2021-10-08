#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include <chrono>
#include <cppfits.hpp>
#include <cstdio>
#include <cstring>

int save_as_fits(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const andor2k::Socket &socket, char *fits_filename,
                 char *socket_buffer) noexcept {

  char buf[32] = {'\0'}; // buffer for datetime string

  // formulate a valid FITS filename to save the data to
  if (get_next_fits_filename(params, fits_filename)) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();
    socket_sprintf(socket, socket_buffer,
                   "done;status:error saving FITS file;error:%d", 1);
    return 1;
  }

  printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...\n",
         date_str(buf), fits_filename);

  // Create a FITS file and save the image at it
  FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
  if (fits.write<at_32>(img_buffer)) {
    fprintf(stderr,
            "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n",
            date_str(buf), __func__);
    socket_sprintf(socket, socket_buffer,
                   "done;error:1;status:error while saving to FITS;error:%d",
                   15);
    return 1;
  } else {
    printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
           fits_filename);
    socket_sprintf(socket, socket_buffer,
                   "info:image saved to FITS;status:FITS file created %s",
                   fits_filename);
  }

  // apply headers to FITS file and close
  if (fits.apply_headers(*fheaders, false) < 0) {
    fprintf(stderr,
            "[WRNNG][%s] Some headers not applied in FITS file! Should inspect "
            "file (traceback: %s)\n",
            date_str(buf), __func__);
  }

  // close the (newly-created) FITS file
  fits.close();

  return 0;
}
