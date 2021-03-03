#include "ccd_params.hpp"
#include "fits_image.hpp"
#include <cassert>
#include <fstream>

int read_data_file(const char *filename, unsigned short *array,
                   long n) noexcept;
void usage() noexcept;

constexpr std::size_t NAXIS1{1024}; // rows
constexpr std::size_t NAXIS2{1024}; // cols
const char *parameter_file = "fitsimage.par";

using aristarchos::CCDParams;
using aristarchos::FitsImage;

int main(int argc, char *argv[]) {

  // check cmd
  if (argc != 2) {
    usage();
    std::cerr << "\n[ERROR] Need to provide an input file\n";
    return 1;
  }

  // initialize a FitsImage instance
  FitsImage fits_out(NAXIS1, NAXIS2, "testimage1.fits");

  // initialize a CCDParams instance for camera properties
  CCDParams camera0;

  // read data off from the input file and store it in the FitsImage instance
  if (read_data_file(argv[1], fits_out.memory_ptr(), NAXIS1 * NAXIS2)) {
    std::cerr << "\n[ERROR] Failed reading input data file";
    return 1;
  }

  // print image data
  fits_out.printFits();

  // update/write header field
  fits_out.updateKey("EXPOSED", (double)15e0, "Exposure time is secs");

  // read camera parameters
  camera0.read_CCD_params(parameter_file);

  return 0;
}

/// @brief Read in data from a file to an array
/// @param[in]  filename The name of the file to be read
/// @param[out] array An unsigned short array to store the elements read in
///             from the input file
/// @return Status of the function at return, that is:
///         Return Value | Status
///         -------------+----------------------------
///                 -1   | Failed to open input file
///                  0   | All ok; n elements read from file
int read_data_file(const char *filename, unsigned short *array,
                   long n) noexcept {
  std::ifstream fin(filename, std::ios::in);
  if (!fin.is_open()) {
    std::cerr << "[ERROR] Cannot open file \"" << filename << "\"";
    return -1;
  }
  long i = 0;
  while (fin >> array[i])
    ++i;
  printf("%ld lines read in from %s\n", i, filename);
  assert(n == i);
  fin.close();
  return 0;
}

/// @brief Print usage info to STDOUT
void usage() noexcept {
  printf("Open a single col text file with ADU from camera\n");
  printf("Usage: fitsimage image.txt\n");
}
