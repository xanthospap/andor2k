#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

constexpr auto MAX_DURATION = 30min;

/// @brief Gracefully shutdown the ANDOR2K camera
/// @see USER’S GUIDE TO SDK, Software Version 2.102
///
/// The shutdown procedure will perform the following tasks:
/// * set the cooler to OFF state
/// * monitor temperature untill we reach SHUTDOWN_TEMPERATURE
/// * call ShutDown
int system_shutdown() noexcept {

  unsigned int status;
  int current_temp;
  char buf[32] = {'\0'}; /* buffer for datetime string */

  // get temperature for reporting
  GetTemperature(&current_temp);
  printf("[DEBUG][%s] Shutting down system ... (temperatue: %+3d)\n",
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
          fprintf(stderr, " System not initialized (traceback: %s)\n", __func__);
          break;
        case DRV_ACQUIRING:
          fprintf(stderr, " Acquisition in progress (traceback: %s)\n", __func__);
          break;
        case DRV_ERROR_ACK:
          fprintf(stderr, " Unable to communicate with card (traceback: %s)\n", __func__);
          break;
        case DRV_NOT_SUPPORTED:
          fprintf(stderr, " Camera does not support switching cooler off (traceback: %s)\n", __func__);
          break;
        default:
          fprintf(stderr, " Undocumented error! (traceback: %s)\n", __func__);
        }
      }
    }
  }

  /* wait untill we reach shutdown temperature */
  printf("[DEBUG][%s] Wating for camera to reach SHUTDOWN_TEMPERATURE ...\n", date_str(buf));
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
    auto elt = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
    
    if ( elt > MAX_DURATION) {
      fprintf("[ERROR][%s] Failed to reach shutdown temperature after 30 minutes (traceback: %s)\n", date_str(), __func__);
      keep_warming = false;
      break;
    } else {
      printf("[DEBUG][%s] Keep warming up ... temperature is %3d, elapsed time: %10d seconds\n", date_str(), current_temp, elt);
    }
  }

  /* shutdown */
  printf("[DEBUG][%s] Shutting down gracefully!\n", date_str(buf));
  std::this_thread::sleep_for(2000ms);
  ShutDown();

  return 0;
}