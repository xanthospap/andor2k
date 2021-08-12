#ifndef __CPP_CFITSIO_ANDOR2K_H__
#define __CPP_CFITSIO_ANDOR2K_H__

#include "fits_header.hpp"
#include "fitsio.h"
#include <cstdint>
#include <cstring>

namespace fits_details {
constexpr int ANDOR2K_MAX_XPIXELS = 2048;
constexpr int ANDOR2K_MAX_YPIXELS = 2048;

/// see https://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/c_user/node20.html
template <typename T> struct cfitsio_bitpix {};
template <> struct cfitsio_bitpix<int8_t> {
  static constexpr int bitpix = 8;
  static constexpr int bscale = 1;
  static constexpr int8_t bzero = -128;
  static_assert(std::numeric_limits<int8_t>::min() <= bzero);
};
template <> struct cfitsio_bitpix<uint16_t> {
  static constexpr int bitpix = 16;
  static constexpr int bscale = 1;
  static constexpr uint16_t bzero = 32768;
  static_assert(std::numeric_limits<uint16_t>::max() >= bzero);
};
template <> struct cfitsio_bitpix<int16_t> {
  static constexpr int bitpix = 16;
  static constexpr int bscale = 1;
  // static constexpr int16_t bzero = 32768;
  // static_assert(std::numeric_limits<int16_t>::max() >= bzero);
};
template <> struct cfitsio_bitpix<uint32_t> {
  static constexpr int bitpix = 32;
  static constexpr int bscale = 1;
  static constexpr uint32_t bzero = 2147483648;
  static_assert(std::numeric_limits<uint32_t>::max() >= bzero);
};
template <> struct cfitsio_bitpix<int32_t> {
  static constexpr int bitpix = 32;
  static constexpr int bscale = 1;
  // static constexpr int32_t bzero = 2147483648;
  // static_assert(std::numeric_limits<int32_t>::max() >= bzero);
};
template <> struct cfitsio_bitpix<uint64_t> {
  static constexpr int bitpix = 64;
  static constexpr int bscale = 1;
  // static constexpr uint64_t bzero = 9223372036854775808;
  // static_assert(std::numeric_limits<uint64_t>::max() >= bzero);
};
template <> struct cfitsio_bitpix<int64_t> {
  static constexpr int bitpix = 32;
  static constexpr int bscale = 1;
  static constexpr int64_t bzero = 2147483648;
  static_assert(std::numeric_limits<int64_t>::max() >= bzero);
};
template <typename T> struct cfitsio_type {};
template <> struct cfitsio_type<signed short> {
  static constexpr int type = TSHORT;
};
template <> struct cfitsio_type<unsigned short> {
  static constexpr int type = TUSHORT;
};
template <> struct cfitsio_type<signed int> {
  static constexpr int type = TINT;
};
template <> struct cfitsio_type<unsigned int> {
  static constexpr int type = TUINT;
};
template <> struct cfitsio_type<signed long> {
  static constexpr int type = TLONG;
};
template <> struct cfitsio_type<unsigned long> {
  static constexpr int type = TULONG;
};

template <> struct cfitsio_type<float> { static constexpr int type = TFLOAT; };
template <> struct cfitsio_type<double> {
  static constexpr int type = TDOUBLE;
};
} // namespace fits_details

///
/// @warning This class can only handle 2-dimensional images
template <typename T> class FitsImage {
private:
  fitsfile *fptr; /* pointer to fits file; defined in fitsio */
  char filename[256];
  int xpixels, ypixels;
  int bitpix = fits_details::cfitsio_bitpix<T>::bitpix;

public:
  FitsImage(const char *fn, int width, int height) noexcept
      : xpixels(width), ypixels(height) {
    std::memset(filename, '\0', 256);
    std::strcpy(filename, fn);
    int status = 0;
    if (fits_create_file(&fptr, filename, &status))
      fits_report_error(stderr, status);
  }

  template <typename S> int write(S *image) noexcept {
    using fits_details::cfitsio_type;
    int status = 0;

    long naxes[] = {ypixels, xpixels};
    if (fits_create_img(fptr, bitpix, 2, naxes, &status))
      fits_report_error(stderr, status);

    if (fits_write_img(fptr, cfitsio_type<S>::type, 1, xpixels * ypixels, image,
                       &status))
      fits_report_error(stderr, status);

    return status;
  }

  int close() noexcept {
    int status = 0;
    if (fits_close_file(fptr, &status))
      fits_report_error(stderr, status);
    return status;
  }

  template <typename K>
  int update_key(const char *keyname, K *value, const char *comment) noexcept {
    using fits_details::cfitsio_type;
    int status = 0;
    if (fits_update_key(fptr, cfitsio_type<K>::type, keyname, value, comment,
                        &status))
      fits_report_error(stderr, status);
    return status;
  }

  int update_key(const char *keyname, const char *value,
                 const char *comment) noexcept {
    int status = 0;
    char cval[FITS_HEADER_VALUE_CHARS];
    std::memset(cval, 0, FITS_HEADER_VALUE_CHARS);
    std::strcpy(cval, value);
    if (fits_update_key(fptr, TSTRING, keyname, cval, comment, &status))
      fits_report_error(stderr, status);
    return status;
  }

  int update_key(const char *keyname, char (&value)[FITS_HEADER_VALUE_CHARS],
                 const char *comment) noexcept {
    int status = 0;
    char cval[FITS_HEADER_VALUE_CHARS];
    std::memset(cval, 0, FITS_HEADER_VALUE_CHARS);
    std::strcpy(cval, value);
    if (fits_update_key(fptr, TSTRING, keyname, cval, comment, &status))
      fits_report_error(stderr, status);
    return status;
  }

  /// @note headers is actually a const parameter, but ... legacy C
  int apply_headers(FitsHeaders &headers, bool stop_if_error) noexcept {
    int hdr_applied = 0;
    int hdr_errors = 0;
    int status;
    for (auto &hdr : headers.mvec) {
      printf("[DEBUG] Applying header %s ...", hdr.key);
      switch (hdr.type) {
      case FitsHeader::ValueType::tchar32:
        status = this->update_key(hdr.key, hdr.cval, hdr.comment);
        break;
      case FitsHeader::ValueType::tint:
        status = this->update_key<int>(hdr.key, &hdr.ival, hdr.comment);
        break;
      case FitsHeader::ValueType::tfloat:
        status = this->update_key<float>(hdr.key, &hdr.fval, hdr.comment);
        break;
      case FitsHeader::ValueType::tuint:
        status = this->update_key<unsigned>(hdr.key, &hdr.uval, hdr.comment);
        break;
      case FitsHeader::ValueType::tdouble:
        status = this->update_key<double>(hdr.key, &hdr.dval, hdr.comment);
        break;
      default:
        status = -100;
      }
      if (status < -99 || (status < 0 && stop_if_error))
        return status;
      if (status < 0) {
        printf("failed!\n");
        --hdr_errors;
      } else if (status > 0) {
        printf("ok\n");
        ++hdr_applied;
      }
    }

    return hdr_errors < 0 ? hdr_errors : hdr_applied;
  }

}; // FitsImage

#endif
