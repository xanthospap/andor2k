#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

char buf[64];
AndorParameters params;

int main() {
  printf("TestAndor: Checking availability of ANDOR2K Camera\n");
  printf("-----------------------------------------------------------------\n");

  // select the camera
  if (select_camera(0) < 0) {
    fprintf(stderr, "[FATAL][%s] Failed to select camera...exiting\n",
            date_str(buf));
    return 10;
  }

  // report daemon initialization
  printf("Initializing ANDOR2K daemon service\n");

  // initialize CCD
  printf("Initializing CCD ....");
  unsigned error = Initialize("/usr/local/etc/andor");
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "\n[FATAL][%s] Initialisation error...exiting\n",
            date_str(buf));
    return 10;
  }
  // allow initialization ... go to sleep for two seconds
  std::this_thread::sleep_for(2000ms);
  printf("... ok!\n");

  printf("ANDOR2K Camera successefully initialized; probing capabilities\n");

  char model[256];
  GetHeadModel(model);
  printf("Type of CCD: %s\n", model);

  int serial;
  GetCameraSerialNumber(&serial);
  printf("Camera Serial Number: %d\n", serial);

  int ad_channels;
  GetNumberADChannels(&ad_channels);
  printf("Number of available AD channels: %d\n", ad_channels);

  int channel = 0, type = 0, num_speeds;
  if (GetNumberHSSpeeds(channel, type, &num_speeds) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to get number of Horizontal Shift "
                    "Speeds for camera!");
  } else {
    float speed;
    printf("Here is a list of available horizontal shift speeds:\n");
    for (int i = 0; i < num_speeds; i++) {
      if (GetHSSpeed(channel, type, i, &speed) != DRV_SUCCESS) {
        fprintf(stderr, "[ERROR][%s] Failed to get HS Speed with index %d\n",
                date_str(buf), i);
      } else {
        printf("\t HSSpeed[%02d] = %.3fMHz\n", i, speed);
      }
    }
  }
  if (GetNumberVSSpeeds(&num_speeds) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to get number of Vertical Shift Speeds "
                    "for camera!");
  } else {
    float speed;
    printf("Here is a list of available vertical shift speeds:\n");
    for (int i = 0; i < num_speeds; i++) {
      if (GetVSSpeed(i, &speed) != DRV_SUCCESS) {
        fprintf(stderr, "[ERROR][%s] Failed to get VS Speed with index %d\n",
                date_str(buf), i);
      } else {
        printf("\t VSSpeed[%02d] = %.3f\n", i, speed);
      }
    }
  }
  int fr_idx;
  float fr_speed;
  if (GetFastestRecommendedVSSpeed(&fr_idx, &fr_speed) == DRV_SUCCESS) {
    printf("Fastest Recommended VS Speed is: %.3f (index %d)\n", fr_speed,
           fr_idx);
  } else {
    fprintf(
        stderr,
        "[ERROR][%s] Failed to get fastest recommended Vertical Shift Speed!");
  }

  printf("-- Checking Camera Gain Controls --\n");
  int noGains;
  if (GetNumberPreAmpGains(&noGains) != DRV_SUCCESS) {
    fprintf(stderr, "ERROR in function GetNumberPreAmpGains\n");
  }
  printf("Number of allowed pre-amp gains: %d\n", noGains);

  for (int i=0; i<noGains; i++) {
    float gain;
    GetPreAmpGain(i, &gain);
    printf("Pre-Amp gain factor for index %d is %.3f\n", i, gain);
  }

  int amp;
  if (GetNumberAmp(&amp) != DRV_SUCCESS) {
    fprintf(stderr, "ERROR in function GetNumberPreAmp\n");
  }
  printf("Number of allowed amp channels: %d\n", amp);

  int adchannels;
  if (GetNumberADChannels(&adchannels) != DRV_SUCCESS) {
    fprintf(stderr, "ERROR in function GetNumberADChannels\n");
  }
  printf("Number of allowed AD channels: %d\n", adchannels);

  for (int channel = 0; channel < adchannels; channel++) {
    int status;
    for (int ampl = 0; ampl < amp; ampl++) {
      for (int idx = 0; idx < 4; idx++) {
        for (int pa = 0; pa < noGains; pa++) {
          int error = IsPreAmpGainAvailable(channel, ampl, idx, pa, &status);
          if (error == DRV_SUCCESS)
            printf("%2d %2d %2d %2d %d\n", channel, ampl, idx, pa, status);
          else {
            if (error == DRV_P1INVALID) {
              fprintf(stderr, "Invalid channel : %d\n", channel);
            } else if (error == DRV_P2INVALID) {
              fprintf(stderr, "Invalid amplifier : %d\n", amp);
            } else if (error == DRV_P3INVALID) {
              fprintf(stderr, "Invalid speed index : %d\n", idx);
            } else if (error == DRV_P4INVALID) {
              fprintf(stderr, "Invalid gain index : %d\n", pa);
            }
          }
        }
      }
    }
  }

  char ampstr[64];
  int n;
  if (GetCurrentPreAmpGain(&n, ampstr, 64) != DRV_SUCCESS) {
    fprintf(stderr, "Funtion GetCurrentPreAmpGain returned error!\n");
  }
  printf("Current Pre-Amp gain for channel: %d is %s\n", n, ampstr);

  int i = 4;
    // std::memset(ampstr, 0, sizeof(char) * 64);
    error = GetAmpDesc(i, ampstr, 21);
    if (error == DRV_P1INVALID) {
      fprintf(stderr, "Invalid amplifier index %d\n", i);
    } else if (error == DRV_SUCCESS) {
      printf("Channel %d, description [%s]\n", ampstr);
    } else {
      fprintf(stderr, "Some kind of error ... \n");
    }
  //}

  // All done, report exit
  printf("Test seems ok, exiting now\n");
  ShutDown();
  return 0;
}
