#include "andor2k.hpp"
#include <cstdio>

int main(int argc, char *argv[]) {
  
  CmdParameters params;
  if (resolve_cmd_parameters(argc, argv, params)) {
    fprintf(stderr, "[ERROR] Failed resolving cmd parameters.\n");
    return 1;
  }

  char fits_fn[256] = {'\0'};
  if ( get_next_fits_filename(&params, fits_fn) ) {
    fprintf(stderr, "[ERROR] Failed to get fits filename ... exiting\n");
  } else {
    printf("Next fits file to be saved, is \"%s\"\n", fits_fn);
  }

  return 0;
}
