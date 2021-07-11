#include "andor2k.hpp"
#include "atmcdLXd.h"

int get_acquisition_kinetic(const AndorParameters *params) noexcept {

  char buf[32] = {'\0'};
  int status;
  int total_images_acquired;
  int first, last;
  int images_in_buffer;
  int images_remaining;
  unsigned int error;

  GetStatus(&status);
  GetTotalNumberImagesAcquired(&total_images_acquired);

  GetNumberNewImages(&first, &last);
  images_in_buffer = last - first;
  images_remaining = params->num_images_ - series;
  error = GetOldestImage(data, pixels);
  if (error != DRV_SUCCESS) {
    if (error == DRV_ERROR_ACK) {
      fprintf(
          stderr,
          "[ERROR][%s] Failed to get image; Unable to communicate with card\n",
          date_str(buf));
    } else if (error == DRV_P1INVALID) {
      fprintf(stderr, "[ERROR][%s] Failed to get image; Invalid data pointer\n",
              date_str(buf));
    } else if (error = DRV_P2INVALID) {
      fprintf(stderr, "[ERROR][%s] Failed to get image; Incorrect array size\n",
              date_str(buf));
    }
  }
}