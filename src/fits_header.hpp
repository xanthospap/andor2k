#ifndef __FITS_HEADER_UTILS_HPP__
#define __FITS_HEADER_UTILS_HPP__

#include <vector>

/// @brief max character in FITS header keyword
constexpr int FITS_HEADER_KEYNAME_CHARS = 16;

/// @brief max character in FITS header value
constexpr int FITS_HEADER_VALUE_CHARS = 32;

/// @brief max character in FITS header comment
constexpr int FITS_HEADER_COMMENT_CHARS = 64;

char *rtrim(char *str) noexcept;

/// @brief Generic FITS header struct
struct FitsHeader {
  enum class ValueType : char {
    tint,
    tuint,
    tfloat,
    tdouble,
    tchar32,
    tlong,
    unknown
  };

  char key[FITS_HEADER_KEYNAME_CHARS];
  char comment[FITS_HEADER_COMMENT_CHARS];
  union {
    char cval[FITS_HEADER_VALUE_CHARS];
    int ival;
    float fval;
    unsigned uval;
    double dval;
    long lval;
  }; // union
  ValueType type;

  void clear() noexcept;

  void cpy_chars(const char *ikey, const char *icomment) noexcept;

}; // FitsHeader

FitsHeader create_fits_header(const char *key, const char *val,
                              const char *comment) noexcept;
FitsHeader create_fits_header(const char *key, int val,
                              const char *comment) noexcept;
FitsHeader create_fits_header(const char *key, float val,
                              const char *comment) noexcept;
FitsHeader create_fits_header(const char *key, double val,
                              const char *comment) noexcept;
FitsHeader create_fits_header(const char *key, unsigned val,
                              const char *comment) noexcept;
FitsHeader create_fits_header(const char *key, long val,
                              const char *comment) noexcept;

struct FitsHeaders {
  std::vector<FitsHeader> mvec;

  FitsHeaders(int size_hint = 100) { mvec.reserve(size_hint); }

  void clear() noexcept { mvec.clear(); }

  int merge(const std::vector<FitsHeader> &hvec,
            bool stop_if_error) noexcept;

  int update(const FitsHeader &hdr) noexcept;

  template <typename T>
  int update(const char *ikey, T tval, const char *icomment) noexcept {
    FitsHeader fh = create_fits_header(ikey, tval, icomment);
    return this->update(fh);
  }

  /// @brief Like update but checks nothing! use with care!
  template <typename T>
  int force_update(const char *ikey, T tval, const char *icomment) noexcept {
    mvec.emplace_back(create_fits_header(ikey, tval, icomment));
    return 1;
  }
}; // FitsHeaders

#endif