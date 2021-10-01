#include "andor_time_utils.hpp"
#include <cstdio>

int main() {
  
  //see https://www.ntppool.org/en/use.html
  const char *ntp_server = "0.pool.ntp.org";

  std_time_point tp;
  int error = get_ntp_time(ntp_server, tp);

  char buf[64];
  if (!error)
    printf("NTP time got from server: %s\n", strfdt<DateTimeFormat::YMDHMfS>(tp, buf));

  return error;
}
