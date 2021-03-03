#include "fits_filename.hpp"
#include <iostream>

using namespace aristarchos;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "\n[ERROR] Need to provide a directory where FITS files live";
    return 1;
  }

  int status;
  char fnstr[64];

  auto fn = get_next_FitsFilename(false, 0, status, argv[1]);
  fn.as_str(&fnstr[0]);

  std::cout << "\nNext file should be: \'" << fnstr << "\', status=" << status;

  fn = get_next_FitsFilename(true, 0, status, argv[1]);
  fn.as_str(&fnstr[0]);
  std::cout << "\nor if we are starting a new MultiRun: \'" << fnstr << "\'"
            << ", status=" << status;

  std::cout << "\nSource directory of the file is \'" << fn.directory << "\'";

  std::cout << "\n";
  return 0;
}
