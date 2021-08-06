#include "fits_header.hpp"
#include <algorithm>
#include <cstring>

/// @brief Right trim a string
char *rtrim(char *str) noexcept {
  char *top = str;
  int sz = std::strlen(str) - 1;
  while (sz >= 0) {
    if (str[sz] != ' ')
      break;
    str[sz] = '\0';
    --sz;
  }
  return top;
}

void FitsHeader::clear() noexcept {
  std::memset(key, 0, FITS_HEADER_KEYNAME_CHARS);
  std::memset(comment, 0, FITS_HEADER_COMMENT_CHARS);
  type = ValueType::unknown;
}

void FitsHeader::cpy_chars(const char *ikey, const char *icomment) noexcept {
  this->clear();
  /* remove leading/trailing whitespace chars from key */
  std::memset(key, 0, FITS_HEADER_KEYNAME_CHARS);
  const char *start = ikey;
  while (*start && *start == ' ')
    ++start;
  std::strcpy(key, start);
  rtrim(key);
  std::strcpy(comment, icomment);
  return;
}

FitsHeader create_fits_header(const char *key, const char *val,
                              const char *comment) noexcept {
  FitsHeader fh;
  fh.cpy_chars(key, comment);
  fh.type = FitsHeader::ValueType::tchar32;
  std::memset(fh.cval, 0, FITS_HEADER_VALUE_CHARS);
  std::strcpy(fh.cval, val);
  return fh;
}
FitsHeader create_fits_header(const char *key, int val,
                              const char *comment) noexcept {
  FitsHeader fh;
  fh.cpy_chars(key, comment);
  fh.type = FitsHeader::ValueType::tint;
  fh.ival = val;
  return fh;
}
FitsHeader create_fits_header(const char *key, float val,
                              const char *comment) noexcept {
  FitsHeader fh;
  fh.cpy_chars(key, comment);
  fh.type = FitsHeader::ValueType::tfloat;
  fh.fval = val;
  return fh;
}
FitsHeader create_fits_header(const char *key, double val,
                              const char *comment) noexcept {
  FitsHeader fh;
  fh.cpy_chars(key, comment);
  fh.type = FitsHeader::ValueType::tdouble;
  fh.dval = val;
  return fh;
}
FitsHeader create_fits_header(const char *key, unsigned val,
                              const char *comment) noexcept {
  FitsHeader fh;
  fh.cpy_chars(key, comment);
  fh.type = FitsHeader::ValueType::tuint;
  fh.uval = val;
  return fh;
}

/// @return If any error has occured, returns the actual number of errors
///         occured during concatenation; if no error has occured, returns
///         the number of headers added
int FitsHeaders::merge(const std::vector<FitsHeader> &hvec,
                       bool stop_if_error) noexcept {

  if (mvec.capacity() < mvec.size() + hvec.size()) {
    std::vector<FitsHeader> newv;
    newv.reserve(mvec.size() + hvec.size());
    newv.swap(mvec);
  }

  int errors = 0;
  int adds = 0;
  for (const auto &hdr : hvec) {
    int failed = this->update(hdr);
    if (failed < 0) {
      if (stop_if_error)
        return -1;
      --errors;
    } else {
      ++adds;
    }
  }
  return errors < 0 ? errors : adds;
}

int FitsHeaders::update(const FitsHeader &hdr) noexcept {
  /* check if key already exists in vector */
  auto it = std::find_if(mvec.begin(), mvec.end(), [&](const FitsHeader &h) {
    return !std::strcmp(h.key, hdr.key);
  });

  /* if key does not exist already, push back new header */
  if (it == mvec.end()) {
    mvec.emplace_back(hdr);
    return 1;
    /* if key exists and its value-type is the same as hdr's value-type,
     * update the value and comment fields
     */
  } else if (it->type == hdr.type &&
             it->type != FitsHeader::ValueType::unknown) {
    *it = hdr;
    return 0;
    /* else (aka key already exists but value types do not match), do
     * nothing and return a negative number
     */
  } else {
    return -1;
  }
}