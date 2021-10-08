#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

/// @brief Max seconds to wait for when shuting down camera
constexpr std::chrono::seconds MAX_DURATION_SEC =
    std::chrono::minutes{MAX_SHUTDOWN_DURATION};

/// @brief Gracefully shutdown the ANDOR2K camera
/// @see USER’S GUIDE TO SDK, Software Version 2.102
///
/// The shutdown procedure will perform the following tasks:
/// * set the cooler to OFF state
/// * monitor temperature untill we reach SHUTDOWN_TEMPERATURE
/// * call ShutDown
///
/// @return Always returns 0
int system_shutdown() noexcept {

  unsigned int status;
  int current_temp;
  char buf[32] = {'\0'}; /* buffer for datetime string */

  int stat;
  GetStatus(&stat);
  if (stat == DRV_ACQUIRING)
    AbortAcquisition();
  char status_str[MAX_STATUS_STRING_SIZE];
  printf("[DEBUG][%s] Shutting down camera; last know state was: %s\n",
         date_str(buf), get_status_string(status_str));

  // get temperature for reporting
  GetTemperature(&current_temp);
  printf("[DEBUG][%s] Shutting down system ... (temperatue: %+3dC)\n",
         date_str(buf), current_temp);

  /*
  // do we need to go to ambient temperature?
  if (params->cooler_mode_) {

    printf("[DEBUG][%s] Setting camera temperature to %+3d C ...\n",
  date_str(buf), SHUTDOWN_TEMPERATURE); status =
  SetTemperature(SHUTDOWN_TEMPERATURE); if (status != DRV_SUCCESS) {
      fprintf(stderr, "[ERROR][%s] Failed to set target temperature!",
  date_str(buf)); switch (status) { case DRV_NOT_INITIALIZED: fprintf(stderr, "
  System is not initialized\n"); break; case DRV_ACQUIRING: fprintf(stderr, "
  Acquisition in progress\n"); break; case DRV_ERROR_ACK: fprintf(stderr, "
  Unable to communicate with card\n"); break; case DRV_P1INVALID:
        fprintf(stderr, " Temperature invalid\n");
        break;
      case DRV_NOT_SUPPORTED:
        fprintf(stderr,
                " The camera does not support setting this temperature\n");
        break;
      default:
        fprintf(stderr, " Undocumented error!\n");
        break;
      }
    }

  } else {
  } */

  /* if cooler is on, close it off */
  int cooler_on;
  status = IsCoolerOn(&cooler_on);
  if (status == DRV_SUCCESS) {
    if (cooler_on) {
      status = CoolerOFF();
      if (status == DRV_SUCCESS) {
        printf("[DEBUG][%s] Cooler is now OFF\n", date_str(buf));
      } else {
        fprintf(stderr, "[ERROR][%s] Failed to shut down cooler!",
                date_str(buf));
        switch (status) {
        case DRV_NOT_INITIALIZED:
          fprintf(stderr, " System not initialized (traceback: %s)\n",
                  __func__);
          break;
        case DRV_ACQUIRING:
          fprintf(stderr, " Acquisition in progress (traceback: %s)\n",
                  __func__);
          break;
        case DRV_ERROR_ACK:
          fprintf(stderr, " Unable to communicate with card (traceback: %s)\n",
                  __func__);
          break;
        case DRV_NOT_SUPPORTED:
          fprintf(
              stderr,
              " Camera does not support switching cooler off (traceback: %s)\n",
              __func__);
          break;
        default:
          fprintf(stderr, " Undocumented error! (traceback: %s)\n", __func__);
        }
      }
    }
  }

  /* wait untill we reach shutdown temperature */
  if (current_temp < SHUTDOWN_TEMPERATURE) {
    printf("[DEBUG][%s] Waiting for camera to reach SHUTDOWN_TEMPERATURE ...\n",
           date_str(buf));
    auto start_time = std::chrono::high_resolution_clock::now();
    bool keep_warming = true;
    while (keep_warming) {
      std::this_thread::sleep_for(5000ms);

      GetTemperature(&current_temp);
      if (current_temp >= SHUTDOWN_TEMPERATURE) {
        keep_warming = false;
        break;
      }

      auto current_time = std::chrono::high_resolution_clock::now();
      auto elt = std::chrono::duration_cast<std::chrono::seconds>(current_time -
                                                                  start_time);

      if (elt > MAX_DURATION_SEC) {
        fprintf(
            stderr,
            "[ERROR][%s] Failed to reach shutdown temperature after %3ld "
            "minutes (traceback: %s)\n",
            date_str(buf),
            std::chrono::duration_cast<std::chrono::minutes>(MAX_DURATION_SEC)
                .count(),
            __func__);
        keep_warming = false;
        break;
      } else {
        printf("[DEBUG][%s] Keep warming up ... temperature is %3d, elapsed "
               "time: %10ld seconds\n",
               date_str(buf), current_temp, elt.count());
      }
    }
  }

  /* shutdown */
  printf("[DEBUG][%s] Shutting down gracefully!\n", date_str(buf));
  std::this_thread::sleep_for(1000ms);
  ShutDown();

  return 0;
}
