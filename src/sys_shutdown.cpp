#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

// constexpr int SHUTDOWN_TEMPERATURE = 2; // shutdown temperature if needed in
// celsius

int system_shutdown() noexcept {

  unsigned int status;
  int current_temp;
  char buf[32] = {'\0'};

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

  // cool_to_temperature(SHUTDOWN_TEMPERATURE);

  // if cooler is on, close it off
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
          fprintf(stderr, " System not initialized\n");
          break;
        case DRV_ACQUIRING:
          fprintf(stderr, " Acquisition in progress\n");
          break;
        case DRV_ERROR_ACK:
          fprintf(stderr, " Unable to communicate with card\n");
          break;
        case DRV_NOT_SUPPORTED:
          fprintf(stderr, " Camera does not support switching cooler off\n");
          break;
        default:
          fprintf(stderr, " Undocumented error!\n");
        }
      }
    }
  }

  printf("[DEBUG][%s] Shutting down gracefully!\n", date_str(buf));
  std::this_thread::sleep_for(2000ms);
  ShutDown();

  return 0;
}