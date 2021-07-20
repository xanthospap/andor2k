#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

/// @brief Max seconds to wait for when shuting down camera
constexpr std::chrono::seconds MAX_DURATION_SEC =
    std::chrono::minutes{MAX_COOLING_DURATION};

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
int cool_to_temperature(int tempC) noexcept {

  unsigned int status;
  int errstat;
  int current_temp;
  char buf[32] = {'\0'}; /* buffer for datetime string */

  /* get current temperature */
  GetTemperature(&current_temp);
  printf("[DEBUG][%s] Current camera temperature is %+3d C\n", date_str(buf),
         current_temp);

  /* set target temperature */
  printf("[DEBUG][%s] Setting camera temperature to %+3d C\n", date_str(buf),
         tempC);
  status = SetTemperature(tempC);
  errstat = 0;
  if (status != DRV_SUCCESS) { /* setting temperature failed! */
    fprintf(stderr, "[ERROR][%s] Failed to set target temperature!", buf);
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, " System is not initialized (traceback: %s)\n", __func__);
      errstat = 1;
      break;
    case DRV_ACQUIRING:
      fprintf(stderr, " Acquisition in progress (traceback: %s)\n", __func__);
      errstat = 2;
      break;
    case DRV_ERROR_ACK:
      fprintf(stderr, " Unable to communicate with card (traceback: %s)\n",
              __func__);
      errstat = 3;
      break;
    case DRV_P1INVALID:
      fprintf(stderr, " Temperature invalid (traceback: %s)\n", __func__);
      errstat = 4;
      break;
    case DRV_NOT_SUPPORTED:
      fprintf(stderr,
              " The camera does not support setting this temperature "
              "(traceback: %s)\n",
              __func__);
      errstat = 5;
      break;
    default:
      fprintf(stderr, " Undocumented error! (traceback: %s)\n", __func__);
      errstat = std::numeric_limits<int>::max();
    }
    return errstat;
  }

  /* set cooling on */
  printf("[DEBUG][%s] Starting cooling process ...\n", date_str(buf));
  errstat = 0;
  status = CoolerON();
  if (status != DRV_SUCCESS) { /* failed to start cooler */
    fprintf(stderr, "[ERROR][%s] Failed to startup the cooler!", buf);
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, " System not initialized! (traceback: %s)\n", __func__);
      errstat = 1;
      break;
    case DRV_ACQUIRING:
      fprintf(stderr, " Acquisition in progress (traceback: %s)\n", __func__);
      errstat = 2;
      break;
    case DRV_ERROR_ACK:
      fprintf(stderr, " Unable to communicate with card (traceback: %s)\n",
              __func__);
      errstat = 3;
      break;
    default:
      fprintf(stderr, " Undocumented error! (traceback: %s)\n", __func__);
      errstat = std::numeric_limits<int>::max();
    }
    return errstat;
  }

  /* wait untill we reach the target temperature */
  status = GetTemperature(&current_temp);
  errstat = 0;
  auto start_time = std::chrono::high_resolution_clock::now();
  while (status != DRV_TEMP_STABILIZED) {
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr,
              "[ERROR][%s] Cooling failed! System not initialized! (traceback: "
              "%s)\n",
              date_str(buf), __func__);
      errstat = 1;
      break;
    case DRV_ACQUIRING:
      fprintf(stderr,
              "[ERROR][%s] Cooling failed! Acquisition in progress (traceback: "
              "%s)\n",
              date_str(buf), __func__);
      errstat = 2;
      break;
    case DRV_ERROR_ACK:
      fprintf(stderr,
              "[ERROR][%s] Cooling failed! Unable to communicate with card "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      errstat = 3;
      break;
    case DRV_TEMP_OFF:
      printf("[DEBUG][%s] Cooler is off\n", date_str(buf));
      break;
    case DRV_TEMP_NOT_REACHED:
      printf("[DEBUG][%s] Cooling down ... temperature at: %+3dC\n",
             date_str(buf), current_temp);
      break;
    case DRV_TEMP_DRIFT:
      printf("[DEBUG][%s] Temperature reached but since drifted ... "
             "temperature at: %+3dC\n",
             date_str(buf), current_temp);
      break;
    case DRV_TEMP_NOT_STABILIZED:
      printf("[DEBUG][%s] Temperature reached but not yet stabilized ... "
             "temperature at: %+3dC\n",
             date_str(buf), current_temp);
      break;
    }
    /* if error occured, exit */
    if (errstat)
      return errstat;

    /* get current time; report and check that we are not taking too long */
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elt = std::chrono::duration_cast<std::chrono::seconds>(current_time -
                                                                start_time);
    if (elt > MAX_DURATION_SEC) {
      fprintf(stderr,
              "[ERROR][%s] Failed to reach temperature after %3ld minutes; "
              "giving up! (traceback: %s)\n",
              date_str(buf),
              std::chrono::duration_cast<std::chrono::minutes>(MAX_DURATION_SEC)
                  .count(),
              __func__);
      errstat = 10;
      return errstat;
    } else {
      printf("[DEBUG][%s] Elpased time while cooling %3ld minutes\n",
             date_str(buf),
             std::chrono::duration_cast<std::chrono::minutes>(current_time -
                                                              start_time)
                 .count());
    }

    std::this_thread::sleep_for(5000ms);
    status = GetTemperature(&current_temp);
  }

  /* all done, return */
  printf("[DEBUG][%s] Temperature reached and stabilized\n", date_str(buf));
  return 0;
}
