#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>

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
  if (num < 0) {
    fprintf(stderr,
            "[ERROR] Invalid camera index number; failed to select camera\n");
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
          "[ERROR] Invalid camera index provided; failed to select camera\n");
      return -1;
    }
  }

  // unreachable
  return -100;
}