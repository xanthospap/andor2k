#include "andor2kd.hpp"
#include "andor2k.hpp"
#include "atmcdLXd.h"
#include "cpp_socket.hpp"
#include "cppfits.hpp"
#include "fits_header.hpp"
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

using andor2k::ServerSocket;
using andor2k::Socket;
using namespace std::chrono_literals;

// ANDOR2K RELATED CONSTANTS
constexpr int ANDOR_MIN_TEMP = -120;
constexpr int ANDOR_MAX_TEMP = 10;

// Global constants for abort/interrupt
extern int sig_abort_set;
extern int sig_interrupt_set;

// buffers and constants for socket communication
constexpr int SOCKET_PORT = 8080;
constexpr int INTITIALIZE_TO_TEMP = -50;
char fits_file[MAX_FITS_FILE_SIZE] = {'\0'};
char now_str[32] = {'\0'}; // YYYY-MM-DD HH:MM:SS
char
    buffer[SOCKET_BUFFER_SIZE];

// ANDOR2K parameters controlling usage
AndorParameters params;

/// @brief Signal handler to kill daemon (calls shutdown() and then exits)
void kill_daemon(int signal) noexcept {
  printf("[DEBUG][%s] Caught signal (#%d); shutting down daemon (traceback: %s)\n",
         date_str(now_str), signal, __func__);
  // system_shutdown(); // RUN
  printf("[DEBUG][%s] Goodbye!\n", date_str(now_str));
  exit(signal);
}
/// @brief Signal handler for SEGFAULT (calls shutdown() and then exits)
void segfault_sigaction(int signal, siginfo_t *si, void *arg) {
  printf("[FATAL][%s] Caught segfault at address %p; shutting down daemon (traceback: %s)\n", date_str(now_str), si->si_addr, __func__);
  // system_shutdown(); // RUN
  printf("[DEBUG][%s] Goodbye!\n", date_str(now_str));
  exit(signal);
}

/// @brief Set ANDOR2K temperature via a command of type: "settemp [ITEMP]"
/// @param[in] command A (char) buffer holding the command to be executed (null
///            terminated). The command should be a c-string of type:
///            "settemp [ITEMP]" where ITEMP is an integer denoting the
///            temperature to be reached by the ANDOR2K camera
int set_temperature(const char *command) noexcept {
  if (std::strncmp(command, "settemp", 7))
    return 1;
  // we expect that after the temperatues, we have a valid int
  char *end;
  int target_temp = std::strtol(command + 7, &end, 10);
  if (end == command + 7 || errno) {
    errno = 0;
    fprintf(
        stderr,
        "[ERROR][%s] Failed to resolve target temperature in command \"%s\"\n",
        date_str(now_str), command);
    fprintf(stderr, "[ERROR][%s] Skippig command \"%s\"\n", now_str, command);
    return 1;
  }
  if (target_temp < ANDOR_MIN_TEMP || target_temp > ANDOR_MAX_TEMP) {
    fprintf(stderr,
            "[ERROR][%s] Refusing to set temperature outside limits: [%+3d, "
            "%+3d]\n",
            date_str(now_str), ANDOR_MIN_TEMP, ANDOR_MAX_TEMP);
    fprintf(stderr, "[ERROR][%s] Skippig command \"%s\"\n", now_str, command);
    return 1;
  }
  // command seems ok .... do it!
  return cool_to_temperature(target_temp);
}

/* OBSOLETE not used anymore */
int acquire_image(AndorParameters &aparams, int xpixels, int ypixels) noexcept {
  /* ------------------------------------------------------------ ACQUISITION */
  printf("[DEBUG][%s] Starting image acquisition ...\n", date_str(now_str));
  StartAcquisition();
  int status;

  if (aparams.acquisition_mode_ == AcquisitionMode::SingleScan) {
    at_32 *imageData = new at_32[xpixels * ypixels];
    std::fstream fout("image.txt", std::ios::out);
    // Loop until acquisition finished
    GetStatus(&status);
    while (status == DRV_ACQUIRING)
      GetStatus(&status);
    GetAcquiredData(imageData, xpixels * ypixels);
    // for (int i = 0; i < xpixels * ypixels; i++)
    //  fout << imageData[i] << std::endl;
    if (get_next_fits_filename(&aparams, fits_file)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved\n",
              date_str(now_str));
    }
    printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...",
           date_str(now_str), fits_file);
    /*if (SaveAsFITS(fits_file, 3) != DRV_SUCCESS) {
      fprintf(stderr, "\n[ERROR][%s] Failed to save image to fits format!\n",
              date_str());
    if (SaveAsFITS(fits_file, 0) != DRV_SUCCESS) {
      fprintf(stderr, "\n[ERROR][%s] Failed to save image to fits format!\n",
              date_str());
    get_next_fits_filename(&aparams, fits_file);*/
    FitsImage<uint16_t> fits(fits_file, xpixels, ypixels);
    if (fits.write<at_32>(imageData)) {
      fprintf(stderr, "[ERROR][%s] Failed writting data to FITS file!\n",
              date_str(now_str));
    } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(now_str),
             fits_file);
    }
    fits.update_key<int>("NXAXIS1", &xpixels, "width");
    fits.update_key<int>("NXAXIS2", &ypixels, "height");
    fits.close();
    /*} else {
      printf(" done!\n");
    }*/
    delete[] imageData;

  } else if (aparams.acquisition_mode_ == AcquisitionMode::RunTillAbort) {
    /*
    at_32 lAcquired = 0;
    at_32 count = 0;
    long lNumberInSeries = 2;//aparams.num_images_;
    at_u16 *data = new unsigned short[xpixels*ypixels];
    while (lAcquired < lNumberInSeries) {
      WaitForAcquisition();
      GetTotalNumberImagesAcquired(&lAcquired);
      GetMostRecentImage16(data, xpixels*ypixels);
      printf("--> Acquired data for image %d/%d\n", lAcquired, lNumberInSeries);
      get_next_fits_filename(&aparams, fits_file);
      unsigned int error = SaveAsFITS(fits_file, 0);
      printf("--> FITS error: %d\n", error);
    }
    AbortAcquisition();
    delete[] data;
    printf("--> Acquisition Aborted\n");
    */
    unsigned int error;
    int images_remaining = aparams.num_images_;
    int buffer_images_remaining = 0;
    int buffer_images_retrieved = 0;
    at_32 series = 0, first, last;
    at_32 *data = new at_32[xpixels * ypixels];
    GetStatus(&status);
    auto acq_start_t = std::chrono::high_resolution_clock::now();
    while ((status == DRV_ACQUIRING && images_remaining > 0) ||
           buffer_images_remaining > 0) {
      if (images_remaining == 0)
        error = AbortAcquisition();

      GetTotalNumberImagesAcquired(&series);

      if (GetNumberNewImages(&first, &last) == DRV_SUCCESS) {
        buffer_images_remaining = last - first;
        images_remaining = aparams.num_images_ - series;

        error = GetOldestImage(data, xpixels * ypixels);

        if (error == DRV_P2INVALID || error == DRV_P1INVALID) {
          fprintf(stderr, "[ERROR][%s] Acquisition error, nr #%u\n",
                  date_str(now_str), error);
          fprintf(stderr, "[ERROR][%s] Aborting acquisition.\n",
                  date_str(now_str));
          AbortAcquisition();
        }

        if (error == DRV_SUCCESS) {
          ++buffer_images_retrieved;
          get_next_fits_filename(&aparams, fits_file);
          FitsImage<uint16_t> fits(fits_file, xpixels, ypixels);
          fits.write<at_32>(data);
          fits.update_key<int>("NXAXIS1", &xpixels, "width");
          fits.update_key<int>("NXAXIS2", &ypixels, "height");
          fits.close();
        }
      }

      // abort after full number of images taken
      if (series >= aparams.num_images_) {
        printf("[DEBUG][%s] Succesefully acquired all images\n",
               date_str(now_str));
        AbortAcquisition();
      }

      GetStatus(&status);
      auto nowt = std::chrono::high_resolution_clock::now();
      double elapsed_time_ms =
          std::chrono::duration<double, std::milli>(nowt - acq_start_t).count();
      if (elapsed_time_ms > 3 * 60 * 1000) {
        fprintf(
            stderr,
            "[ERROR][%s] Aborting acquisition cause it took too much time!\n",
            date_str(now_str));
        AbortAcquisition();
        GetStatus(&status);
      }
    }
    /*
    at_32 *data = new at_32[xpixels * ypixels];
    int images_acquired = 0;
    bool abort = false;
    auto sleep_ms = std::chrono::milliseconds(
        static_cast<int>(std::ceil(aparams.exposure_ + 0.5)) * 2000);
    std::this_thread::sleep_for(sleep_ms);
    while ((images_acquired < aparams.num_images_) && !abort) { //
    -----------------------------------> unsigned int error =
    GetOldestImage(data, xpixels * ypixels); switch (error) { case DRV_SUCCESS:
        ++images_acquired;
        printf("[DEBUG][%s] Acquired image nr %2d/%2d\n", date_str(),
               images_acquired, aparams.num_images_);
        if (get_next_fits_filename(&aparams, fits_file)) {
          fprintf(
              stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved\n",
              date_str());
        }
        printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...",
               date_str(), fits_file);
        unsigned int fits_error;
        std::this_thread::sleep_for(1000ms);
        if (fits_error = SaveAsFITS(fits_file, 3); fits_error != DRV_SUCCESS) {
          fprintf(stderr,
                  "\n[ERROR][%s] Failed to save image to fits format!",
                  date_str());
          if (fits_error == DRV_ERROR_ACK) {
            fprintf(stderr, " unable to communicate with card\n");
          } else if (fits_error == DRV_P1INVALID) {
            fprintf(stderr, " invalid pointer\n");
          } else if (fits_error == DRV_P2INVALID) {
            fprintf(stderr, " array size is incorrect\n");
          } else if (fits_error == DRV_NO_NEW_DATA) {
            fprintf(stderr, " no new data yet\n");
          }
        } else {
          printf(" done!\n");
        }
        break;
      case DRV_NOT_INITIALIZED:
        fprintf(stderr,
                "[ERROR][%s] Failed to acquire image; system not initialized\n",
                date_str());
        [[fallthrough]];
      case DRV_ERROR_ACK:
        fprintf(stderr,
                "[ERROR][%s] Failed to acquire image; unable to "
                "communicate with card\n",
                date_str());
        [[fallthrough]];
      case DRV_P1INVALID:
        fprintf(stderr,
                "[ERROR][%s] Failed to acquire image; data array size is "
                "incorrect\n",
                date_str());
        AbortAcquisition();
        abort = true;
        break;
      case DRV_NO_NEW_DATA:
        printf("[DEBUG][%s] No new data yet ... waiting ...\n", date_str());
        std::this_thread::sleep_for(sleep_ms);
      }
    } // <----------------------------------- delete[] data;
    */
  }
  /* ------------------------------------------------------------ ACQUISITION */

  return 0;
}

int get_image(const char *command) noexcept {

  /* first try to resolve the image parameters of the command */
  if (resolve_image_parameters(command, params)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to resolve image parameters; aborting request! "
            "(traceback: %s)\n",
            date_str(now_str), __func__);
    return 1;
  }

  /* setup the acquisition process for the image(s); also prepare FITS headers
   * for later use in the file(s) to be saved
   */
  int width, height;
  float vsspeed, hsspeed;
  FitsHeaders fheaders;
  at_32 *data = nullptr; /* remember to free this */
  int status = 0;
  if (setup_acquisition(&params, &fheaders, width, height, vsspeed, hsspeed,
                        data)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to setup acquisition; aborting request! "
            "(traceback: %s)\n",
            date_str(now_str), __func__);
    status = 2;
  }
  
  if (!status) {
    if (status = get_acquisition(&params, &fheaders, width, height, data);
        status != 0) {
      fprintf(stderr,
              "[ERROR][%s] Failed to get/save image(s); aborting request now "
              "(traceback: %s)\n",
              date_str(now_str), __func__);
      status = 3;
    }
  }

  /* free memory and return */
  delete[] data;
  return status;
}

int resolve_command(const char *command) noexcept {
  if (!(std::strncmp(command, "settemp", 7))) {
    return set_temperature(command);
  } else if (!(std::strncmp(command, "shutdown", 8))) {
    return -100;
  } else if (!(std::strncmp(command, "status", 6))) {
    return print_status();
  } else if (!(std::strncmp(command, "image", 5))) {
    return get_image(command);
  } else {
    fprintf(stderr,
            "[ERROR][%s] Failed to resolve command: \"%s\"; doing nothing!\n",
            date_str(now_str), command);
    return 1;
  }
}

void chat(const Socket &socket) {
  for (;;) {

    /* read message from client into buffer */
    std::memset(buffer, '\0', sizeof(buffer));
    socket.recv(buffer, 1024);

    /* perform the operation requested by clinet */
    int answr = resolve_command(buffer);
    if (answr == -100) {
      printf(
          "[DEBUG][%s] Received shutdown command; initializing exit sequence\n",
          date_str(now_str));
      break;
    }

    // get message for client
    buffer[0] = '0';
    buffer[1] = '\0';
    if (answr)
      buffer[0] = '1';

    // send message to client
    socket.send(buffer);
  }

  return;
}

int main() {
  int sock_status;
  unsigned int error;

  // register signal for SEGFAULT
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(struct sigaction));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segfault_sigaction;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);

  // register signals and accossiated and signal handlers
  signal(SIGINT, kill_daemon);
  signal(SIGQUIT, kill_daemon);
  signal(SIGTERM, kill_daemon);

  // select the camera
  if (select_camera(params.camera_num_) < 0) {
    fprintf(stderr, "[FATAL][%s] Failed to select camera...exiting\n",
            date_str(now_str));
    return 10;
  }

  /* report daemon initialization */
  printf("[DEBUG][%s] Initializing ANDOR2K daemon service\n",
         date_str(now_str));

  // initialize CCD
  printf("[DEBUG][%s] Initializing CCD ....", date_str(now_str));
  error = Initialize(params.initialization_dir_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "[FATAL][%s] Initialisation error...exiting\n",
            date_str(now_str));
    return 10;
  }
  // allow initialization ... go to sleep for two seconds
  std::this_thread::sleep_for(2000ms);
  printf("... ok!\n");

  // cool down if needed RUN
  /*
  if (cool_to_temperature(INTITIALIZE_TO_TEMP)) {
      fprintf(stderr, "[FATAL][%s] Failed to set target
  temperature...exiting\n", date_str()); return 10;
  }*/

  try {
    ServerSocket server_sock(SOCKET_PORT);

    printf("[DEBUG][%s] Listening on port %d\n", date_str(now_str),
           SOCKET_PORT);
    printf("[DEBUG][%s] Service is up and running ... waiting for input\n",
           date_str(now_str));

    /* creating hearing child socket */
    Socket child_socket = server_sock.accept(sock_status);
    if (sock_status < 0) {
      fprintf(stderr, "[FATAL][%s] Failed to create child socket ... exiting\n",
              date_str(now_str));
      return 1;
    }
    printf("[DEBUG][%s] Waiting for instructions ...\n", date_str(now_str));

    /* communicate with client */
    chat(child_socket);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR][%s] Failed creating deamon\n", date_str(now_str));
    fprintf(stderr, "[FATAL][%s] ... exiting\n", date_str(now_str));
  }

  /* shutdown system */
  system_shutdown();

  return 0;
}
