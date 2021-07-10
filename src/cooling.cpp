#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

int cool_to_temperature(int tempC) noexcept {

  unsigned int status;
  int current_temp;
  char tbuf[32] = {'\0'};

  // get current temperature
  GetTemperature(&current_temp);
  printf("[DEBUG][%s] Current camera temperature is %+3d C\n", date_str(tbuf), current_temp);

  // set cooler mode (keep or not temperature at shutdown)
  /*
  printf("[DEBUG][%s] Setting cooler mode to %1d\n", date_str(tbuf), (int)params->cooler_mode_);
  status = SetCoolerMode(params->cooler_mode_);
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to set cooler mode!", tbuf);
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
  */

  // set target temperature
  printf("[DEBUG][%s] Setting camera temperature to %+3d C ...\n", date_str(tbuf), 
         tempC);
  status = SetTemperature(tempC);
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to set target temperature!", tbuf);
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
  printf("[DEBUG][%s] Starting cooling process ...\n", date_str(tbuf));
  status = CoolerON();
  if (status != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to startup the cooler!", tbuf);
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
      fprintf(stderr, "[ERROR][%s] Cooling failed! System not initialized!\n", date_str(tbuf));
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, "[ERROR][%s] Cooling failed! Acquisition in progress\n", date_str(tbuf));
      return 2;
    case DRV_ERROR_ACK:
      fprintf(stderr,
              "[ERROR][%s] Cooling failed! Unable to communicate with card\n", date_str(tbuf));
      return 3;
    case DRV_TEMP_OFF:
      printf("[DEBUG][%s] Cooler is off\n", date_str(tbuf));
      break;
    case DRV_TEMP_NOT_REACHED:
      printf("[DEBUG][%s] Cooling down ... temperature at: %+3d\n", date_str(tbuf), current_temp);
      break;
    case DRV_TEMP_DRIFT:
      printf("[DEBUG][%s] Temperature reached but since drifted ... temperature at: %+3d\n", date_str(tbuf), current_temp);
      break;
    case DRV_TEMP_NOT_STABILIZED:
      printf("[DEBUG][%s] Temperature reached but not yet stabilized ... temperature at: %+3d\n", date_str(tbuf), current_temp);
      break;
    }
    std::this_thread::sleep_for(5000ms);
    status = GetTemperature(&current_temp);
  }

  printf("[DEBUG][%s] Temperature reached and stabilized\n", date_str(tbuf));
  // all done, return
  return 0;
}