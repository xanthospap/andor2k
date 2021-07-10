#include "andor2k.hpp"
#include <cstdio>

int main(int argc, char *argv[]) {
  AndorParameters params;
  if (resolve_cmd_parameters(argc, argv, params)) {
    fprintf(stderr, "[ERROR] Failed resolving cmd parameters.\n");
    return 1;
  }

  printf("This is what was resolved:\n");
  printf("\tNum of Images: %3d\n", params.num_images_);
  printf("\tBin window h.: %3d\n", params.image_hbin_);
  printf("\tBin window v.: %3d\n", params.image_vbin_);
  printf("\tExposure     : %5.2f\n", params.exposure_);
  printf("\tType         : %s\n", params.type_);

  return 0;
}