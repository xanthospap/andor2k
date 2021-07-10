#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

int cool_to_temperature(const CmdParameters *params) noexcept {

  unsigned int status;
  int current_temp;

  // get current temperature
  GetTemperature(&current_temp);
  printf("[DEBUG] Current camera temperature is %+3d C\n", current_temp);

  // set cooler mode (keep or not temperature at shutdown)
  printf("[DEBUG] Setting cooler mode to %1d\n", (int)params->cooler_mode_);
  status = SetCoolerMode(params->cooler_mode_);
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to set cooler mode!");
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, " System is not initialized\n");
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, " Acquisition in progress\n");
      return 2;
    case DRV_P1INVALID:
      fprintf(stderr, " Temperature invalid\n");
      return 4;
    case DRV_NOT_SUPPORTED:
      fprintf(stderr, " The camera does not support this mode\n");
      return 5;
    default:
      fprintf(stderr, " Undocumented error!\n");
      return std::numeric_limits<unsigned int>::max();
    }
  }

  // set target temperature
  printf("[DEBUG] Setting camera temperature to %+3d C ...\n",
         params->target_temperature_);
  status = SetTemperature(params->target_temperature_);
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to set target temperature!");
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, " System is not initialized\n");
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, " Acquisition in progress\n");
      return 2;
    case DRV_ERROR_ACK:
      fprintf(stderr, " Unable to communicate with card\n");
      return 3;
    case DRV_P1INVALID:
      fprintf(stderr, " Temperature invalid\n");
      return 4;
    case DRV_NOT_SUPPORTED:
      fprintf(stderr,
              " The camera does not support setting this temperature\n");
      return 5;
    default:
      fprintf(stderr, " Undocumented error!\n");
      return std::numeric_limits<unsigned int>::max();
    }
  }

  // set on cooling
  printf("[DEBUG] Starting cooling process ...\n");
  status = CoolerON();
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to startup the cooler!");
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, " System not initialized!\n");
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, " Acquisition in progress\n");
      return 2;
    case DRV_ERROR_ACK:
      fprintf(stderr, " Unable to communicate with card\n");
      return 3;
    default:
      fprintf(stderr, " Undocumented error!\n");
      return std::numeric_limits<unsigned int>::max();
    }
  }

  // wait untill we reach the target temperature
  status = GetTemperature(&current_temp);
  while (status != DRV_TEMP_STABILIZED) {
    switch (status) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, "[ERROR] Cooling failed! System not initialized!\n");
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, "[ERROR] Cooling failed! Acquisition in progress\n");
      return 2;
    case DRV_ERROR_ACK:
      fprintf(stderr,
              "[ERROR] Cooling failed! Unable to communicate with card\n");
      return 3;
    case DRV_TEMP_OFF:
      printf("[DEBUG] Cooler is off\n");
      break;
    case DRV_TEMP_NOT_REACHED:
      printf("[DEBUG] Cooling down ... temperature at: %+3d\n", current_temp);
      break;
    case DRV_TEMP_DRIFT:
      printf("[DEBUG] Temperature reached but since drifted ... temperature at: %+3d\n", current_temp);
      break;
    case DRV_TEMP_NOT_STABILIZED:
      printf("[DEBUG] Temperature reached but not yet stabilized ... temperature at: %+3d\n", current_temp);
      break;
    }
    std::this_thread::sleep_for(3000ms);
    status = GetTemperature(&current_temp);
  }

  printf("[DEBUG] Temperature reached and stabilized\n");
  // all done, return
  return 0;
}