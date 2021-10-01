#ifndef __ANDOR2K_DATETIME_UTILITIES_HPP__
#define __ANDOR2K_DATETIME_UTILITIES_HPP__

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

using std_time_point = std::chrono::system_clock::time_point;

enum class DateTimeFormat : char { YMD, YMDHMfS, YMDHMS, HMS, HMfS };


int get_ntp_time(const char *ntp_server, std_time_point& ntpt) noexcept;

double start_time_correction_impl(float exposure, float vsspeed, float hsspeed,
                                  int img_rows, int img_cols) noexcept;

std::chrono::nanoseconds start_time_correction(float exposure, float vsspeed,
                                               float hsspeed, int img_rows,
                                               int img_cols) noexcept;

std::tm strfdt_work(const std_time_point &t, long &fractional_seconds) noexcept;

template <DateTimeFormat F>
char *strfdt(const std_time_point &t, char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

template <>
inline char *strfdt<DateTimeFormat::YMD>(const std_time_point &t,
                                         char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

template <>
inline char *strfdt<DateTimeFormat::YMDHMS>(const std_time_point &t,
                                            char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%FT%T", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

template <>
inline char *strfdt<DateTimeFormat::YMDHMfS>(const std_time_point &t,
                                             char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%FT%T.", &tm); btw > 0) {
    sprintf(buf + btw, "%03ld", fsec);
    return buf;
  }
  return nullptr;
}

template <>
inline char *strfdt<DateTimeFormat::HMfS>(const std_time_point &t,
                                          char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%T.", &tm); btw > 0) {
    sprintf(buf + btw, "%03ld", fsec);
    return buf;
  }
  return nullptr;
}

template <>
inline char *strfdt<DateTimeFormat::HMS>(const std_time_point &t,
                                         char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%T.", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

#endif
