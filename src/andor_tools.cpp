#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <ctime>

const char *date_str(char* buf) noexcept {
  std::time_t now = std::time(nullptr);
  std::tm *now_loct = ::localtime(&now);
  std::strftime(buf, 32, "%D %T", now_loct);
  return buf;
}

int print_status() noexcept {
  char buf[32]={'\0'};
  int status;
  
  if (GetStatus(&status) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to get camera status!\n", date_str(buf));
    return 1;
  }

  date_str(buf);
  switch (status) {
    case DRV_IDLE:
      printf("[STATUS][%s] IDLE; waiting for instructions\n", buf);
      break;
    case DRV_TEMPCYCLE:
      printf("[STATUS][%s] Executing temperature cycle\n", buf);
      break;
    case DRV_ACQUIRING:
      printf("[STATUS][%s] Acquisition in progress\n", buf);
      break;
    case DRV_ACCUM_TIME_NOT_MET:
      printf("[STATUS][%s] Unable to meet Accumulate cycle time\n", buf);
      break;
    case DRV_KINETIC_TIME_NOT_MET:
      printf("[STATUS][%s] Unable to meet Kinetic cycle time\n", buf);
      break;
    case DRV_ERROR_ACK:
      printf("[STATUS][%s] Unable to communicate with card\n", buf);
      break;
    case DRV_ACQ_BUFFER:
      printf("[STATUS][%s] Computer unable to read the data via the ISA slot at the required rate\n", buf);
      break;
    case DRV_ACQ_DOWNFIFO_FULL:
      printf("[STATUS][%s] Computer unable to read data fast enough to stop camera memory going full\n", buf);
      break;
    case DRV_SPOOLERROR:
      printf("[STATUS][%s] Overflow of the spool buffer\n", buf);
      break;
  }

  unsigned int error;
  int ctemp;
  date_str(buf);
  error = GetTemperature(&ctemp);
  switch (error) {
    case DRV_NOT_INITIALIZED:
      fprintf(stderr, "[STATUS][%s] Temp: %+4dC: System not initialized!\n", buf, ctemp);
      return 1;
    case DRV_ACQUIRING:
      fprintf(stderr, "[STATUS][%s] Temp: %+4dC: Acquisition in progress\n", buf, ctemp);
      return 2;
    case DRV_ERROR_ACK:
      fprintf(stderr,
              "[STATUS][%s] Temp: %+4dC: Unable to communicate with card\n", buf, ctemp);
      return 3;
    case DRV_TEMP_OFF:
      printf("[STATUS][%s] Temp: %+4dC: Temperature is off\n", buf, ctemp);
      break;
    case DRV_TEMP_NOT_REACHED:
      printf("[STATUS][%s] Temp: %+4dC: Temperature has not reached set point\n", buf, ctemp);
      break;
    case DRV_TEMP_DRIFT:
      printf("[STATUS][%s] Temp: %+4dC: Temperature had stabilised but has since drifted\n", buf, ctemp);
      break;
    case DRV_TEMP_NOT_STABILIZED:
      printf("[STATUS][%s] Temp: %+4dC: Temperature reached but not stabilized\n", buf, ctemp);
      break;
    case DRV_TEMP_STABILIZED:
      printf("[STATUS][%s] Temp: %+4dC: Temperature has stabilized at set point.\n", buf, ctemp);
      break;
  }

  return 0;
}

/// @brief Get handle and select Camera
/// @param[in] num Camera Index (only needed in case multiple cameras are
///                present; else, set num to 0)
/// If more than one cameras are present, and the camera index is valid, the
/// function will call:
/// * GetCameraHandle, and
/// * SetCurrentCamera
/// @return the index of the selected camera; on sucess, the input parameter
///         num. In case of error, the function will return an negative
///         integer.
int select_camera(int num) noexcept {
  char buf[32]={'\0'};
  if (num < 0) {
    fprintf(stderr,
            "[ERROR][%s] Invalid camera index number; failed to select camera\n", date_str(buf));
    return -1;
  } else if (num == 0) {
    return num;
  } else {
    at_32 lNumCameras;
    GetAvailableCameras(&lNumCameras);
    int iSelectedCamera = num;

    if (iSelectedCamera < lNumCameras && iSelectedCamera >= 0) {
      at_32 lCameraHandle;
      GetCameraHandle(iSelectedCamera, &lCameraHandle);
      SetCurrentCamera(lCameraHandle);
      return iSelectedCamera;
    } else {
      fprintf(
          stderr,
          "[ERROR][%s] Invalid camera index provided; failed to select camera\n", date_str(buf));
      return -1;
    }
  }

  // unreachable
  return -100;
}