#include "andor2k.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int resolve_cmd_parameters(int argc, char *argv[],
                           CmdParameters &params) noexcept {
  char *end;
  int i = 1;
  while (i < argc) {

    /* BINNING ACCUMULATION (AKA IMAGES) -------------------------------------*/
    if (!std::strncmp(argv[i], "--nimages", 9)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--nimages\"\n");
        return 1;
      }
      params.num_images_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1] || params.num_images_ < 1) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to (valid) "
                "integral numeric value\n",
                argv[i + 1]);
        return 1;
      }
      if (params.num_images_ > 1) {
        params.acquisition_mode_ = AcquisitionMode::KineticSeries;
        printf("[DEBUG] Setting Acquisition Mode to %1d\n",
               AcquisitionMode2int(params.acquisition_mode_));
      }
      i += 2;

      /* BINNING OPTIONS
       * -------------------------------------------------------*/
    } else if (!std::strncmp(argv[i], "--bin", 5)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--bin\"\n");
        return 1;
      }
      params.image_vbin_ = std::strtol(argv[i + 1], &end, 10);
      params.image_hbin_ = params.image_vbin_;
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else if (!std::strncmp(argv[i], "--hbin", 6)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--hbin\"\n");
        return 1;
      }
      params.image_hbin_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else if (!std::strncmp(argv[i], "--vbin", 6)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--vbin\"\n");
        return 1;
      }
      params.image_vbin_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;

      /* IMAGE DIMENSIONS OPTIONS
       * ----------------------------------------------*/
    } else if (!std::strncmp(argv[i], "--hstart", 8)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--hstart\"\n");
        return 1;
      }
      params.image_hstart_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else if (!std::strncmp(argv[i], "--hend", 6)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--hend\"\n");
        return 1;
      }
      params.image_hend_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else if (!std::strncmp(argv[i], "--vstart", 8)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--vstart\"\n");
        return 1;
      }
      params.image_vstart_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else if (!std::strncmp(argv[i], "--vend", 6)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a numeric argument to \"--vend\"\n");
        return 1;
      }
      params.image_vend_ = std::strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1]) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to integral "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;

      /* IMAGE FILENAME
       * --------------------------------------------------------*/
    } else if (!std::strncmp(argv[i], "--filename", 10)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a string argument to \"--filename\"\n");
        return 1;
      }
      if (std::strlen(argv[i + 1]) >= 128) {
        fprintf(stderr, "[ERROR] Invalid argument for \"%s\"\n", argv[i]);
        return 1;
      }
      std::memcpy(params.image_filename_, argv[i + 1],
                  std::strlen(argv[i + 1]));
      i += 2;

    } else if (!std::strncmp(argv[i], "--type", 6)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a string argument to \"--type\"\n");
        return 1;
      }
      if (std::strlen(argv[i + 1]) > 15) {
        fprintf(stderr, "[ERROR] Invalid argument for \"%s\"\n", argv[i]);
        return 1;
      }
      std::memcpy(params.type_, argv[i + 1], std::strlen(argv[i + 1]));
      i += 2;
    } else if (!std::strncmp(argv[i], "--exposure", 10)) {
      if (argc <= i + 1) {
        fprintf(stderr,
                "[ERROR] Must provide a float argument to \"--exposure\"\n");
        return 1;
      }
      params.exposure_ = std::strtod(argv[i + 1], &end);
      if (end == argv[i + 1] || !(params.exposure_ > 0e0)) {
        fprintf(stderr,
                "[ERROR] Failed to convert parameter \"%s\" to a (valid) float "
                "numeric value\n",
                argv[i + 1]);
        return 1;
      }
      i += 2;
    } else {
      fprintf(stderr, "[WARNING] Ignoring input parameter \"%s\"\n", argv[i]);
      ++i;
    }
  }
  return 0; // all ok
}