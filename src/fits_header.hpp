#ifndef __FITS_HEADER_UTILS_HPP__
#define __FITS_HEADER_UTILS_HPP__

#include <vector>

/// @brief max character in FITS header keyword
constexpr int FITS_HEADER_KEYNAME_CHARS = 16;

/// @brief max character in FITS header value
constexpr int FITS_HEADER_VALUE_CHARS = 32;

/// @brief max character in FITS header comment
constexpr int FITS_HEADER_COMMENT_CHARS = 64;

/// @brief Generic FITS header struct
struct FitsHeader {
  char key[FITS_HEADER_KEYNAME_CHARS];
  char val[FITS_HEADER_VALUE_CHARS];
  char comment[FITS_HEADER_COMMENT_CHARS];
};

char *rtrim(char *str) noexcept;

struct FitsHeaders {
    std::vector<FitsHeader> mvec;
    
    FitsHeaders(int size_hint=100) {
        mvec.reserve(size_hint);
    }

    void clear() noexcept {
        mvec.clear();
    }

    int update(const char* ikey, const char* ival, const char* icomment) noexcept;
};

#endif