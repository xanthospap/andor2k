#include "fits_header.hpp"
#include <cstdio>
#include <cstring>

void print_headers(const FitsHeaders &hdrs) noexcept {
  int count = 0;
  printf("<--- Printing Headers --->\n");
  for (auto const &h : hdrs.mvec) {
    switch (h.type) {
    case FitsHeader::ValueType::tchar32:
      printf("[%2d] %8s = %32s / %s\n", count, h.key, h.cval, h.comment);
      break;
    case FitsHeader::ValueType::tfloat:
      printf("[%2d] %8s = %32.5f / %s\n", count, h.key, h.fval, h.comment);
      break;
    case FitsHeader::ValueType::tdouble:
      printf("[%2d] %8s = %32.5f / %s\n", count, h.key, h.dval, h.comment);
      break;
    case FitsHeader::ValueType::tint:
      printf("[%2d] %8s = %32d / %s\n", count, h.key, h.ival, h.comment);
      break;
    case FitsHeader::ValueType::tuint:
      printf("[%2d] %8s = %32u / %s\n", count, h.key, h.uval, h.comment);
      break;
    default:
      fprintf(stderr, "ERROR don;t know how to print header!\n");
    }
    ++count;
  }
  return;
}

int main() {
  FitsHeaders headers;

  headers.update("KEY1", "VALUE1", "my first comment!");
  headers.update("  KEY2", "VALUE2", "my second comment!");
  headers.update("  KEY3 ", "VALUE3", "my third comment!");
  headers.update("KEY4  ", "VALUE4", "my fourth comment!");
  headers.update(" KEY5 ", "VALUE5", "my fifth comment!");

  char k[64], v[64], c[64];
  std::strcpy(k, " KEY6  ");
  std::strcpy(v, "29873498237");
  std::strcpy(c, "yet another comment");
  if (headers.update(k, v, c) < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  std::memset(k, 0, 64);
  std::memset(v, 0, 64);
  std::memset(c, 0, 64);
  std::strcpy(k, " KEY1  ");
  std::strcpy(v, "1");
  std::strcpy(c, "should replace comment 1 (changed)");
  if (headers.update(k, v, c) < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  std::memset(k, 0, 64);
  std::memset(v, 0, 64);
  std::memset(c, 0, 64);
  std::strcpy(k, " KEY1123  ");
  std::strcpy(v, "1123");
  std::strcpy(c, "comment lost count!!");
  if (headers.update(k, v, c) < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  std::memset(k, 0, 64);
  std::memset(v, 0, 64);
  std::memset(c, 0, 64);
  std::strcpy(k, " KEY5  ");
  std::strcpy(v, "1123");
  std::strcpy(c, "changed key5");
  if (headers.update(k, v, c) < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  // lets try adding headers of other types
  if (headers.update<int>("  Key91", 10, "an integer value") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }
  if (headers.update<unsigned>("Key92", 10, "an unsigned integer value") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }
  if (headers.update<float>("Key93", 10.0, "a float value") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }
  if (headers.update<double>("  Key94  ", 10.1, "a double value") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  if (headers.update<double>(" Key94 ", 10.2, "a double value (changed!)") <
      0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }
  if (headers.update<int>("  Key91", 11, "an integer value (changed!)") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  // lets do something erronoous
  // key92 has an unsigned value
  if (headers.update<int>(
          "Key92", 10, "an signed(!) integer value; should be an error") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }
  // key93 has a float value
  if (headers.update<double>("Key93", 10e0,
                             "a double(!) value; should be an error") < 0) {
    fprintf(stderr, "Failed adding header at line %d\n", __LINE__);
  }

  print_headers(headers);

  printf("\n");
  return 0;
}