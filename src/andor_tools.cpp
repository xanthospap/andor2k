#include "andor2k.hpp"
#include "atmcdLXd.h"
#include "cpp_socket.hpp"
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

int print_status(const andor2k::Socket& sock) noexcept {
  char buf[32] = {'\0'}; // buffer for datetime string
  char descr[MAX_STATUS_STRING_SIZE];
  char sockbuf[MAX_SOCKET_BUFFER_SIZE];
  int cbytes;
  
  // get and report status
  printf("[DEBUG][%s] Status report for ANDOR2K:\n", date_str(buf));
  printf("[DEBUG][%s] %s\n", buf, get_status_string(descr));

  cbytes = sprintf(sockbuf, "status:%s;", descr);

  // get and report temperature
  unsigned int error;
  int ctemp;
  date_str(buf);
  error = GetTemperature(&ctemp);
  printf("[DEBUG][%s] Temp: %+4dC: %s\n", buf, ctemp, get_get_temperature_string(error, descr));
  cbytes += sprintf(sockbuf+cbytes, "temp:%+4d (%s);", ctemp, descr);
  
  // report end of status
  printf("[DEBUG][%s] End of status report for ANDOR2K:\n", date_str(buf));

  sprintf(sockbuf+cbytes, "time:%s;", date_str(buf));
  if (sock.send(sockbuf)<1)
    printf("[ERROR][%s] Failed to send status report to client (socket fd %d)\n", date_str(buf), sock.sockid());
  else
    printf("[DEBUG][%s] Sent status report to client: [%s] (socket fd %d)\n", date_str(buf), sockbuf, sock.sockid());

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
    fprintf(stderr,
            "[ERROR][%s] Invalid camera index number; failed to select camera "
            "(traceback: %s)\n",
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
