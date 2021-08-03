#ifndef __FITS_HEADER_UTILS_HPP__
#define __FITS_HEADER_UTILS_HPP__

#include <vector>
#include <cstring>
#include <algorithm>

/// @brief max character in FITS header keyword
constexpr int FITS_HEADER_KEYNAME_CHARS = 16;

/// @brief max character in FITS header value
constexpr int FITS_HEADER_VALUE_CHARS = 32;

/// @brief max character in FITS header comment
constexpr int FITS_HEADER_COMMENT_CHARS = 64;

char *rtrim(char *str) noexcept;

/// @brief Generic FITS header struct
struct FitsHeader {
  enum class ValueType : char { tint, tuint, tfloat, tdouble, tchar32, unknown };
  
  char key[FITS_HEADER_KEYNAME_CHARS];
  char comment[FITS_HEADER_COMMENT_CHARS];
  union {
    char cval[FITS_HEADER_VALUE_CHARS];
    int  ival;
    float fval;
    unsigned uval;
    double dval;
  };// union
  ValueType type;
  
  void clear() noexcept {
    std::memset(key, 0, FITS_HEADER_KEYNAME_CHARS);
    std::memset(comment, 0, FITS_HEADER_COMMENT_CHARS);
    type = ValueType::unknown;
  }

  void cpy_chars(const char* ikey, const char* icomment) noexcept {
    this->clear();
    /* remove leading/trailing whitespace chars from key */
    std::memset(key, 0, FITS_HEADER_KEYNAME_CHARS);
    const char* start = ikey;
    while (*start && *start == ' ') ++start;
    std::strcpy(key, start);
    rtrim(key);
    std::strcpy(comment, icomment);
    return;
  }

  /// @brief compra two FitsHeaders by key
  /// @return The following integers are returned:
  ///    - keys are the same and values are of the same type : 0
  ///    - keys are the same and values are of different types : -1
  ///    - keys are not the same : 1
  /*
  int compare(const FitsHeader& other) noexcept {
      if (!std::strcmp(key, other.key)) {
          return type == other.type ? 0 : -1;
      }
      return 1;
  }*/
}; // FitsHeader

/*
/// @brief (Template) factory function to create a FitsHeaderInstance
template<typename T> FitsHeader create_fits_header(const char* key, T val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::unknown;
    fh.dval = val;
    return fh;
}
template<> FitsHeader create_fits_header<const char*>(const char* key, const char* val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tchar32;
    std::memset(fh.cval, 0, FITS_HEADER_VALUE_CHARS);
    std::strcpy(fh.cval, val);
    return fh;
}
template<> FitsHeader create_fits_header<int>(const char* key, int val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tint;
    fh.ival = val;
    return fh;
}
template<> FitsHeader create_fits_header<float>(const char* key, float val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tfloat;
    fh.fval = val;
    return fh;
}
template<> FitsHeader create_fits_header<double>(const char* key, double val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tdouble;
    fh.dval = val;
    return fh;
}
template<> FitsHeader create_fits_header<unsigned>(const char* key, unsigned val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tfloat;
    fh.fval = val;
    return fh;
}
*/
FitsHeader create_fits_header(const char* key, const char* val, const char* comment) noexcept;
FitsHeader create_fits_header(const char* key, int val, const char* comment) noexcept;
FitsHeader create_fits_header(const char* key, float val, const char* comment) noexcept;
FitsHeader create_fits_header(const char* key, double val, const char* comment) noexcept;
FitsHeader create_fits_header(const char* key, unsigned val, const char* comment) noexcept;

struct FitsHeaders {
    std::vector<FitsHeader> mvec;
    
    FitsHeaders(int size_hint=100) {
        mvec.reserve(size_hint);
    }

    void clear() noexcept {
        mvec.clear();
    }

    template <typename T>
    int update(const char *ikey, T tval, const char *icomment) noexcept {
      FitsHeader fh = create_fits_header(ikey, tval, icomment);

      auto it =
          std::find_if(mvec.begin(), mvec.end(), [&](const FitsHeader &hdr) {
            return !std::strcmp(hdr.key, fh.key);
          });

      if (it == mvec.end()) {
        mvec.emplace_back(fh);
        return 1;
      } else if (it->type == fh.type &&
                 it->type != FitsHeader::ValueType::unknown) {
        *it = fh;
        return 0;
      } else {
        return -1;
      }
    }
}; // FitsHeaders

#endif