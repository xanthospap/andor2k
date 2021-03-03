#ifndef __ARISTARCHOS_FITSIMAGE_HPP__
#define __ARISTARCHOS_FITSIMAGE_HPP__

#include "cfitsio/fitsio.h"
#include <cstring>
#include <iostream>
#include <stdio.h>

namespace aristarchos {

class FitsImage {
public:
  /// @note Could throw if memory allocation fails
  FitsImage(long nrows, long ncols, const char *fn)
      : m_fpixel(1), m_bitpix(USHORT_IMG), m_nelements(nrows * ncols),
        m_status(0), m_naxis(2), m_array(nullptr) {
    m_naxes[0] = ncols;
    m_naxes[1] = nrows;
    std::strcpy(m_filename, fn);
    m_array = new unsigned short[m_nelements + 1];
  }

  /// @brief Destructor; free memory allocated by m_array
  ~FitsImage() {
    if (m_array)
      delete[] m_array;
    m_array = nullptr;
    fits_close_file(m_fptr, &m_status);
  }

  /// @brief Copy constructor disabled
  FitsImage(const FitsImage &image) = delete;

  /// @brief Copy assignment operator disabled
  FitsImage &operator=(const FitsImage &image) = delete;

  /// @brief Move assignment
  FitsImage(FitsImage &&image) noexcept
      : m_fpixel(image.m_fpixel), m_bitpix(image.m_bitpix),
        m_nelements(image.m_nelements), m_status(image.m_status),
        m_naxis(image.m_naxis), m_array(image.m_array) {
    m_naxes[0] = image.m_naxes[0];
    m_naxes[1] = image.m_naxes[1];
    std::strcpy(m_filename, image.m_filename);
    image.m_array = nullptr;
    image.m_naxes[0] = image.m_naxes[1] = image.m_nelements = 0;
  }

  /// @brief Move assignment operator
  FitsImage &operator=(FitsImage &&image) noexcept {
    if (this != &image) {
      m_fpixel = image.m_fpixel;
      m_bitpix = image.m_bitpix;
      m_nelements = image.m_nelements;
      m_status = image.m_status;
      m_naxis = image.m_naxis;
      m_array = image.m_array;
      m_naxes[0] = image.m_naxes[0];
      m_naxes[1] = image.m_naxes[1];
      std::strcpy(m_filename, image.m_filename);
      image.m_array = nullptr;
      image.m_naxes[0] = image.m_naxes[1] = image.m_nelements = 0;
    }
    return *this;
  }

  int updateKey(const char *keyname, double hdrdata,
                const char *comment) noexcept {
    // Sets the header info. Applied AFTER data written, else SEGFAULT
    fits_update_key(m_fptr, TDOUBLE, keyname, &hdrdata, comment, &m_status);
    return m_status;
  }

  void PrintImageStats(void) const;

  /// @brief Set all elements in m_array to zero
  void zeroImage() noexcept { std::memset(m_array, 0, m_nelements); }

  int printFits() noexcept {
    // remove(filename); TODO
    // create new file
    fits_create_file(&m_fptr, m_filename, &m_status);
    // Create the primary array image (16-bit unsigned short integer pixels)
    fits_create_img(m_fptr, m_bitpix, m_naxis, m_naxes, &m_status);
    // Write the array of integers to the image
    fits_write_img(m_fptr, TUSHORT, m_fpixel, m_nelements, m_array, &m_status);
    // print out any errormessages
    fits_report_error(stderr, m_status);
    // return the status
    return m_status;
  }

  /// @brief Copy m_nelements from data to m_array
  void fillImage(const int *data) noexcept {
    std::copy(data, data + m_nelements, m_array);
  }

  /// @brief Return a pointer to the allocated memory
  unsigned short *memory_ptr() noexcept { return m_array; }

private:
  fitsfile *m_fptr; // pointer to the FITS file, defined in fitsio.h
  long m_fpixel;
  int m_bitpix;     // USHORT_IMG; * 16-bit unsigned short pixel values
  long m_nelements; // number of elements (aka pixels); rows*cols
  int m_status;
  long m_naxis;         // 2 a 2-dimensional image
  long m_naxes[2];      // ncols, nrows
  char m_filename[256]; // name for new FITS file
  unsigned short *m_array;
}; // FitsImage

} // namespace aristarchos
#endif
