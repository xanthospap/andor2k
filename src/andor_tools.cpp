#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <ctime>

/// @brief Fill input buffer buf with current local datetime "%Y-%m-%d %H:%M:%S"
/// @param[in] The input buffer to store the datetime string; must be of size
///            >= 32.
/// @return A c-string holding current local datetime; this is actually the
///         input string buf.
const char *date_str(char *buf) noexcept {
  std::time_t now = std::time(nullptr);
  std::tm *now_loct = std::localtime(&now);
  std::strftime(buf, 32, "%F %T", now_loct);
  return buf;
}

int print_status() noexcept {
  char buf[32] = {'\0'}; /* buffer for datetime string */
  int status;

  /* get status */
  if (GetStatus(&status) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to get camera status! (traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }

  /* report status */
  printf("[DEBUG][%s] Status report for ANDOR2K:\n", date_str(buf));

  date_str(buf);
  switch (status) {
  case DRV_IDLE:
    printf("[DEBUG][%s] IDLE; waiting for instructions\n", buf);
    break;
  case DRV_TEMPCYCLE:
    printf("[DEBUG][%s] Executing temperature cycle\n", buf);
    break;
  case DRV_ACQUIRING:
    printf("[DEBUG][%s] Acquisition in progress\n", buf);
    break;
  case DRV_ACCUM_TIME_NOT_MET:
    printf("[DEBUG][%s] Unable to meet Accumulate cycle time\n", buf);
    break;
  case DRV_KINETIC_TIME_NOT_MET:
    printf("[DEBUG][%s] Unable to meet Kinetic cycle time\n", buf);
    break;
  case DRV_ERROR_ACK:
    printf("[DEBUG][%s] Unable to communicate with card\n", buf);
    break;
  case DRV_ACQ_BUFFER:
    printf("[DEBUG][%s] Computer unable to read the data via the ISA slot at "
           "the required rate\n",
           buf);
    break;
  case DRV_ACQ_DOWNFIFO_FULL:
    printf("[DEBUG][%s] Computer unable to read data fast enough to stop "
           "camera memory going full\n",
           buf);
    break;
  case DRV_SPOOLERROR:
    printf("[DEBUG][%s] Overflow of the spool buffer\n", buf);
    break;
  }

  /* get and report temperature */
  unsigned int error;
  int ctemp;
  date_str(buf);
  error = GetTemperature(&ctemp);
  switch (error) {
  case DRV_NOT_INITIALIZED:
    fprintf(stderr, "[DEBUG][%s] Temp: %+4dC: System not initialized!\n", buf,
            ctemp);
    return 1;
  case DRV_ACQUIRING:
    fprintf(stderr, "[DEBUG][%s] Temp: %+4dC: Acquisition in progress\n", buf,
            ctemp);
    return 2;
  case DRV_ERROR_ACK:
    fprintf(stderr,
            "[DEBUG][%s] Temp: %+4dC: Unable to communicate with card\n", buf,
            ctemp);
    return 3;
  case DRV_TEMP_OFF:
    printf("[DEBUG][%s] Temp: %+4dC: Temperature is off\n", buf, ctemp);
    break;
  case DRV_TEMP_NOT_REACHED:
    printf("[DEBUG][%s] Temp: %+4dC: Temperature has not reached set point\n",
           buf, ctemp);
    break;
  case DRV_TEMP_DRIFT:
    printf("[DEBUG][%s] Temp: %+4dC: Temperature had stabilised but has since "
           "drifted\n",
           buf, ctemp);
    break;
  case DRV_TEMP_NOT_STABILIZED:
    printf("[DEBUG][%s] Temp: %+4dC: Temperature reached but not stabilized\n",
           buf, ctemp);
    break;
  case DRV_TEMP_STABILIZED:
    printf(
        "[DEBUG][%s] Temp: %+4dC: Temperature has stabilized at set point.\n",
        buf, ctemp);
    break;
  }
  
  /* report end of status */
  printf("[DEBUG][%s] End of status report for ANDOR2K:\n", date_str(buf));

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
  char buf[32] = {'\0'};
  if (num < 0) {
    fprintf(
        stderr,
        "[ERROR][%s] Invalid camera index number; failed to select camera (traceback: %s)\n",
        date_str(buf), __func__);
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
      fprintf(stderr,
              "[ERROR][%s] Invalid camera index provided; failed to select "
              "camera (traceback: %s)\n",
              date_str(buf), __func__);
      return -1;
    }
  }

  // unreachable
  return -100;
}