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

FitsHeader create_fits_header(const char* key, const char* val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tchar32;
    std::memset(fh.cval, 0, FITS_HEADER_VALUE_CHARS);
    std::strcpy(fh.cval, val);
    return fh;
}
FitsHeader create_fits_header(const char* key, int val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tint;
    fh.ival = val;
    return fh;
}
FitsHeader create_fits_header(const char* key, float val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tfloat;
    fh.fval = val;
    return fh;
}
FitsHeader create_fits_header(const char* key, double val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tdouble;
    fh.dval = val;
    return fh;
}
FitsHeader create_fits_header(const char* key, unsigned val, const char* comment) noexcept {
    FitsHeader fh;
    fh.cpy_chars(key, comment);
    fh.type = FitsHeader::ValueType::tuint;
    fh.uval = val;
    return fh;
}

/*
int FitsHeaders::update(const char* ikey, const char* ival, const char* icomment) noexcept {
    
    // remove leading/trailing whitespace chars from key
    char key[FITS_HEADER_KEYNAME_CHARS];
    std::memset(key, 0, FITS_HEADER_KEYNAME_CHARS);
    const char* start = ikey;
    while (*start && *start == ' ') ++start;
    std::strcpy(key, start);
    rtrim(key);

    auto it = std::find_if(mvec.begin(), mvec.end(), [&](const FitsHeader& hdr){ return !std::strcmp(hdr.key, key); });

    if (it == mvec.end()) {
        FitsHeader newhdr;
        std::memcpy(newhdr.key, key, FITS_HEADER_KEYNAME_CHARS);
        std::strcpy(newhdr.val, ival);
        std::strcpy(newhdr.comment, icomment);
        mvec.emplace_back(newhdr);
        return 1;
    } else {
        std::strcpy(it->val, ival);
        std::strcpy(it->comment, icomment);
        return 0;
    }

    return -1;
}
*/