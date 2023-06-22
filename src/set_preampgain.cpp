#include "andor2k.hpp"
#include "aristarchos.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>

int set_preampgain(const AndorParameters &params) noexcept {
  char buf[32] = {'\0'}; // buffer for datetime string
  int preampgain_index = params.preampgain;

  /* make sure passed in index is ok */
  int noGains;
  if (GetNumberPreAmpGains(&noGains) != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting number of PreAmpGain from camera! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }

  if (preampgain_index < 0 || preampgain_index >= noGains) {
    fprintf(stderr,
            "[ERROR][%s] Invalid Pre-Amp Gain index (%d); doing nothing!"
            "(traceback: %s)\n",
            date_str(buf), preampgain_index, __func__);
    return 1;
  }

  if (SetPreAmpGain(preampgain_index) != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed setting PreAmpGain index to %d; doing nothing!"
            "(traceback: %s)\n",
            date_str(buf), preampgain_index, __func__);
    return 1;
  }

  float factor;
  if (GetPreAmpGain(preampgain_index, &factor) != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed retrieving Pre-Amp Gain; wierd ..."
            "(traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }
  printf("[DEBUG][%s] Set Pre-Amp gain factor to %.1fx\n", date_str(buf),
         factor);
  return 0;
}
