#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

constexpr int SHUTDOWN_TEMPERATURE =
    2; // shutdown temperature if needed in celsius

int system_shutdown(const AndorParameters *params) noexcept {

  unsigned int status;
  int current_temp;

  // get temperature for reporting
  GetTemperature(&current_temp);

  printf("[DEBUG] Shutting down system ... (temperatue: %+3d)\n", current_temp);

  // if cooler is on, close it off
  int cooler_on;
  status = IsCoolerOn(&cooler_on);
  if (status == DRV_SUCCESS) {
    if (cooler_on) {
      status = CoolerOFF();
      if (status == DRV_SUCCESS) {
        printf("[DEBUG] Cooler is now OFF\n");
      } else {
        fprintf(stderr, "[ERROR] Failed to shut down cooler!");
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

  // do we need to go to ambient temperature?
  if (params->cooler_mode_) {

    printf("[DEBUG] Setting camera temperature to %+3d C ...\n",
           SHUTDOWN_TEMPERATURE);
    status = SetTemperature(SHUTDOWN_TEMPERATURE);
    if (status != DRV_SUCCESS) {
      fprintf(stderr, "[ERROR] Failed to set target temperature!");
      switch (status) {
      case DRV_NOT_INITIALIZED:
        fprintf(stderr, " System is not initialized\n");
        break;
      case DRV_ACQUIRING:
        fprintf(stderr, " Acquisition in progress\n");
        break;
      case DRV_ERROR_ACK:
        fprintf(stderr, " Unable to communicate with card\n");
        break;
      case DRV_P1INVALID:
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

    /*
    while (current_temp < 0 && current_temp > -999) {
      std::this_thread::sleep_for(3000ms);
      status = GetTemperature(&current_temp);
    }
    */

  } // cooler_mode_ is true

  printf("[DEBUG] Shutting down gracefully!\n");
  std::this_thread::sleep_for(2000ms);
  ShutDown();

  return 0;
}