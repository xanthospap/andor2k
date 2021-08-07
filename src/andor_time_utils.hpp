#ifndef __ANDOR2K_DATETIME_UTILITIES_HPP__
#define __ANDOR2K_DATETIME_UTILITIES_HPP__

#include <thread>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cmath>

using std_time_point = std::chrono::system_clock::time_point;

enum class DateTimeFormart : char { YMD, YMDHMfS, YMDHMS, HMS, HMfS };

double start_time_correction_impl(float exposure, float vsspeed, float hsspeed,
                             int img_rows, int img_cols) noexcept;

std::chrono::nanoseconds start_time_correction(float exposure, float vsspeed, float hsspeed,
                             int img_rows, int img_cols) noexcept;

std::tm strfdt_work(const std_time_point& t, long& fractional_seconds) noexcept;

template<DateTimeFormart F>
char *strfdt(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm)>0) {
    return buf;
  }
  return nullptr;
}

template<> inline
char *strfdt<DateTimeFormart::YMD>(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm)>0) {
    return buf;
  }
  return nullptr;
}

template<> inline
char *strfdt<DateTimeFormart::YMDHMS>(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%FT%T", &tm)>0) {
    return buf;
  }
  return nullptr;
}

template<> inline
char *strfdt<DateTimeFormart::YMDHMfS>(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%FT%T.", &tm); btw>0) {
    sprintf(buf+btw, "%03ld", fsec);
    return buf;
  }
  return nullptr;
}

template<> inline
char *strfdt<DateTimeFormart::HMfS>(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%T.", &tm); btw>0) {
    sprintf(buf+btw, "%03ld", fsec);
    return buf;
  }
  return nullptr;
}

template<> inline
char *strfdt<DateTimeFormart::HMS>(const std_time_point& t, char* buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%T.", &tm)>0) {
    return buf;
  }
  return nullptr;
}

#endif