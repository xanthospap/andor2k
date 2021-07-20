#include "andor2k.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc, char *argv[]) {

  AndorParameters params;

  if (resolve_cmd_parameters(argc, argv, params)) {
    fprintf(stderr, "[ERROR] Failed resolving cmd parameters.\n");
    return 1;
  }

  std::strcpy(params.save_dir_, "/home/xanthos/Builds/andor2k/bin");

  char fits_fn[256] = {'\0'};

  for (int i = 0; i < 10; i++) {
    if (get_next_fits_filename(&params, fits_fn)) {
      fprintf(stderr, "[ERROR] Failed to get fits filename ... exiting\n");
      return 1;
    }

    printf("Next fits file to be saved, is \"%s\"\n", fits_fn);
    std::ofstream fout(fits_fn);
    if (!fout.is_open()) {
      fprintf(stderr, "[ERROR] Failed opening file \"%s\"\n", fits_fn);
      return 2;
    }
    fout << "Dummy fits file\n";
    fout << "Please delete me!";
    fout.close();
  }

  return 0;
}
