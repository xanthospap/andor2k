#ifndef __ANDOR2K_DATETIME_UTILS_HPP__
#define __ANDOR2K_DATETIME_UTILS_HPP__

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <time.h>

using stdc_time_point = std::chrono::system_clock::time_point;

enum class DateTimeFormart : char { YMD, YMDHMfS, YMDHMS, HMS, HMfS };

std::tm strfdt_work(const stdc_time_point &t,
                    long &fractional_seconds) noexcept {
  std::chrono::milliseconds ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          t.time_since_epoch());
  std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm tm = *std::gmtime(&tt); // GMT (UTC)
  fractional_seconds = ms.count() % 1000;
  return tm;
}

template <DateTimeFormart F>
char *strfdt(const stdc_time_point &t, char *buf) noexcept {
  return nullptr;
}

template <>
char *strfdt<DateTimeFormart::YMD>(const stdc_time_point &t,
                                   char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%F", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

template <>
char *strfdt<DateTimeFormart::YMDHMS>(const stdc_time_point &t,
                                      char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%FT%T", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

template <>
char *strfdt<DateTimeFormart::YMDHMfS>(const stdc_time_point &t,
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
char *strfdt<DateTimeFormart::HMfS>(const stdc_time_point &t,
                                    char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (int btw = std::strftime(buf, 64, "%T.", &tm); btw > 0) {
    sprintf(buf + btw, "%09ld", fsec);
    return buf;
  }
  return nullptr;
}

template <>
char *strfdt<DateTimeFormart::HMS>(const stdc_time_point &t,
                                   char *buf) noexcept {
  long fsec;
  auto tm = strfdt_work(t, fsec);
  if (std::strftime(buf, 64, "%T.", &tm) > 0) {
    return buf;
  }
  return nullptr;
}

#endif