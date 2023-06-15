#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "atmcdLXd.h"
#include "cpp_socket.hpp"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

/// @brief Cool down the ANDOR2K camera using the cooler
/// @see USER’S GUIDE TO SDK, Software Version 2.102
///
/// The cooling procedure will perform the following tasks:
/// * get current temperature (report)
/// * set target temperature
/// * set camera cooler on
/// * wait untill target temperature is reached
///
/// If any of the above steps fails, the procedure is aborted WITHOUT SETTING
/// THE COOLER TO OFF (STATE).
///
/// @return On success returns 0; any other integer denotes an error.
int cool_to_temperature(int tempC, const andor2k::Socket *socket) noexcept {

  unsigned int status;
  int current_temp;
  char buf[32] = {'\0'};             // buffer for datetime string
  char sbuf[MAX_SOCKET_BUFFER_SIZE]; // for reporting to socket, if needed

  // get current temperature
  GetTemperature(&current_temp);
  printf("[DEBUG][%s] Current camera temperature is %+3dC\n", date_str(buf),
         current_temp);
  if (socket)
    socket_sprintf(*socket, sbuf,
                   "ctemp:%d;status:got current camera temperature",
                   current_temp);

  // set target temperature
  printf("[DEBUG][%s] Setting camera temperature to %+3dC\n", date_str(buf),
         tempC);
  status = SetTemperature(tempC);
  if (status != DRV_SUCCESS) { // setting temperature failed!
    fprintf(stderr,
            "[ERROR][%s] Failed to set target temperature! (traceback: %s)\n",
            buf, __func__);
    if (socket)
      socket_sprintf(
          *socket, sbuf,
          "done;error:%u;ctemp:%d;status:Failed to set target temperature!",
          status, current_temp);
    return 1;
  }

  // set cooling on
  printf("[DEBUG][%s] Starting cooling process ...\n", date_str(buf));
  status = CoolerON();
  if (status != DRV_SUCCESS) { // failed to start cooler
    fprintf(stderr, "[ERROR][%s] Failed to startup the cooler! (traceback: %s)",
            buf, __func__);
    if (socket)
      socket_sprintf(
          *socket, sbuf,
          "done;error:%u;ctemp:%d;status:Failed to startup the cooler!", status,
          current_temp);
    return 1;
  }

  // wait untill we reach the target temperature
  status = GetTemperature(&current_temp);
  auto start_time = std::chrono::high_resolution_clock::now();
  char status_str[MAX_STATUS_STRING_SIZE];
  while (status != DRV_TEMP_STABILIZED) {
    get_get_temperature_string(status, status_str);
    
    if (status == DRV_TEMP_NOT_REACHED || status == DRV_TEMP_DRIFT ||
        status == DRV_TEMP_NOT_STABILIZED || DRV_TEMP_OFF) {
      printf("[DEBUG][%s] Temperature: %+4dC; %s\n", date_str(buf),
             current_temp, status_str);
      if (socket)
        socket_sprintf(*socket, sbuf, "ctemp:%d;status:%s (%u)", current_temp,
                       status_str, status);
    } else {
      fprintf(stderr, "[ERROR][%s] Cooling failed! %s (traceback: %s)\n",
              date_str(buf), status_str, __func__);
      if (socket)
        socket_sprintf(*socket, sbuf, "done;error:%u;ctemp:%d;status:%s",
                       status, current_temp, status_str);
      return 1;
    }

    // get current time; report and check that we are not taking too long
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elt = std::chrono::duration_cast<std::chrono::minutes>(current_time -
                                                                start_time)
                   .count();
    if (elt > MAX_COOLING_DURATION) {
      fprintf(stderr,
              "[ERROR][%s] Failed to reach temperature after %3ld minutes; "
              "giving up! (traceback: %s)\n",
              date_str(buf), elt, __func__);
      if (socket)
        socket_sprintf(*socket, sbuf,
                       "done;error:%u;ctemp:%d;status:Failed to reach "
                       "temperature after %3ld minutes",
                       1, current_temp, elt);
      return 1;
    } else {
      printf("[DEBUG][%s] Elapsed time while cooling %3ld minutes\n",
             date_str(buf), elt);
    }

    std::this_thread::sleep_for(5000ms);
    status = GetTemperature(&current_temp);
  }

  // all done, return
  printf("[DEBUG][%s] Temperature reached and stabilized\n", date_str(buf));
  if (socket)
    socket_sprintf(
        *socket, sbuf,
        "done;ctemp:%d;status:Target temperature reached and stabilized",
        current_temp);
  return 0;
}
