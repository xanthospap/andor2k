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

// Global constants for abort/interrupt
extern int sig_abort_set;
extern int sig_interrupt_set;
extern int abort_exposure_set;

// buffers and constants for socket communication
constexpr int INTITIALIZE_TO_TEMP = -50;
char fits_file[MAX_FITS_FILE_SIZE] = {'\0'};
char now_str[32] = {'\0'}; // YYYY-MM-DD HH:MM:SS
char buffer[MAX_SOCKET_BUFFER_SIZE];

/// @brief Signal handler to kill daemon (calls shutdown() and then exits)
void kill_daemon(int signal) noexcept {
  printf(
      "[DEBUG][%s] Caught signal (#%d); shutting down daemon (traceback: %s)\n",
      date_str(now_str), signal, __func__);
  system_shutdown(); // RUN
  printf("[DEBUG][%s] Goodbye!\n", date_str(now_str));
  exit(signal);
}
/// @brief Signal handler for SEGFAULT (calls shutdown() and then exits)
void segfault_sigaction(int signal, siginfo_t *si, void *arg) {
  printf("[FATAL][%s] Caught segfault at address %p; shutting down daemon "
         "(traceback: %s)\n",
         date_str(now_str), si->si_addr, __func__);
  system_shutdown(); // RUN
  printf("[DEBUG][%s] Goodbye!\n", date_str(now_str));
  exit(signal);
}

/// @brief Set ANDOR2K temperature via a command of type: "settemp [ITEMP]"
/// @param[in] command A (char) buffer holding the command to be executed (null
///            terminated). The command should be a c-string of type:
///            "settemp [ITEMP]" where ITEMP is an integer denoting the
///            temperature to be reached by the ANDOR2K camera
int set_temperature(const char *command, const Socket &socket) noexcept {
  errno = 0;
  char buffer[MAX_SOCKET_BUFFER_SIZE];
  if (std::strncmp(command, "settemp", 7)) {
    socket_sprintf(socket, buffer, "done;error:1;status:Invalid command!");
    return 1;
  }
  
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
    socket_sprintf(socket, buffer, "done;error:1;status:Invalid command!");
    return 1;
  }
  
  if (target_temp < ANDOR_MIN_TEMP || target_temp > ANDOR_MAX_TEMP) {
    fprintf(stderr,
            "[ERROR][%s] Refusing to set temperature outside limits: [%+3d, "
            "%+3d]\n",
            date_str(now_str), ANDOR_MIN_TEMP, ANDOR_MAX_TEMP);
    fprintf(stderr, "[ERROR][%s] Skippig command \"%s\"\n", now_str, command);
    socket_sprintf(socket, buffer, "done;error:1;status:Invalid command!");
    return 1;
  }
  
  // command seems ok .... do it!
  return cool_to_temperature(target_temp, &socket);
}

int get_image(const char *command, const Socket &socket,
              AndorParameters &params) noexcept {

  // first try to resolve the image parameters of the command
  if (resolve_image_parameters(command, params)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to resolve image parameters; aborting request! "
            "(traceback: %s)\n",
            date_str(now_str), __func__);
    return 1;
  }

  // setup the acquisition process for the image(s); also prepare FITS headers
  // for later use in the file(s) to be saved
  int width, height;
  float vsspeed, hsspeed;
  FitsHeaders fheaders;
  at_32 *data = nullptr; // remember to free this
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
    if (status =
            get_acquisition(&params, &fheaders, width, height, data, socket);
        status != 0) {
      fprintf(stderr,
              "[ERROR][%s] Failed to get/save image(s); aborting request now "
              "(traceback: %s)\n",
              date_str(now_str), __func__);
      status = 3;
    }
  }

  // free memory and return
  delete[] data;
  return status;
}

int set_param_value(const char *command, AndorParameters &params) noexcept {
  if (std::strncmp(command, "setparam", 8))
    return 1;

  // copy the input string so that we can tokenize it
  char string[MAX_SOCKET_BUFFER_SIZE];
  std::memcpy(string, command, MAX_SOCKET_BUFFER_SIZE);

  int ival;
  float fval;

  char *token = std::strtok(string, " "); // this is the command (aka setparam)
  token = std::strtok(nullptr, " ");
  char *start, *end;
  // split remaining substring to tokens and process one at a time
  while (token) {

    if (!std::strncmp(token, "acqmode=", 8)) {
      ival = std::strtol(token + 8, &end, 10);
      if (end == token) {
        return 10;
      }
      params.acquisition_mode_ = static_cast<AcquisitionMode>(ival);
      printf("[DEBUG][%s] Changing Acquisition Mode to : %d!\n",
             date_str(now_str), ival);

    } else if (!std::strncmp(token, "kineticcycletime=", 17)) {
      fval = std::strtod(token + 17, &end);
      if (end == token) {
        return 10;
      }
      params.kinetics_cycle_time_ = fval;
      printf("[DEBUG][%s] Changing Kinetic Cycle Time to : %.3fsec!\n",
             date_str(now_str), fval);
    } else {
      fprintf(stderr,
              "[WRNNG][%s] Skipping token in paramter set command: [%s]\n",
              date_str(now_str), token);
    }

    token = std::strtok(nullptr, " "); // resolve next token (if any)
  }

  return 0;
}

int resolve_command(const char *command, const Socket &socket,
                    AndorParameters &params) noexcept {
  if (!(std::strncmp(command, "settemp", 7))) {
    return set_temperature(command, socket);
  } else if (!(std::strncmp(command, "shutdown", 8))) {
    return -100;
  } else if (!(std::strncmp(command, "status", 6))) {
    // report here and also send to client
    return print_status(socket);
  } else if (!(std::strncmp(command, "setparam", 8))) {
    return set_param_value(command, params);
  } else if (!(std::strncmp(command, "image", 5))) {
    return get_image(command, socket, params);
  } else if (!(std::strncmp(command, "abort", 5))) {
    abort_exposure_set = true;
    return 0;
  } else {
    fprintf(stderr,
            "[ERROR][%s] Failed to resolve command: \"%s\"; doing nothing!\n",
            date_str(now_str), command);
    return 1;
  }
}

void chat(const Socket &socket, AndorParameters &params) {
  int bytes_r;

  for (;;) {

    // read message from client into buffer
    std::memset(buffer, 0, MAX_SOCKET_BUFFER_SIZE);
    bytes_r = socket.recv(buffer, 1024);

    if (bytes_r < 1) return;

    // perform the operation requested by client
    int answr = resolve_command(buffer, socket, params);
    if (answr == -100) {
      printf(
          "[DEBUG][%s] Received shutdown command; initializing exit sequence\n",
          date_str(now_str));
      break;
    }
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

  // ANDOR2K parameters controlling usage
  AndorParameters params;

  // initialize AndorParameters
  params.set_defaults();
  if (params.read_out_mode_ != ReadOutMode::Image) {
    printf("WTF?? main ~199 readoutmode is not image\n");
    return 10;
  }

  // select the camera
  if (select_camera(params.camera_num_) < 0) {
    fprintf(stderr, "[FATAL][%s] Failed to select camera...exiting\n",
            date_str(now_str));
    return 10;
  }

  // report daemon initialization
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

    // creating hearing child socket
    Socket child_socket = server_sock.accept(sock_status);
    if (sock_status < 0) {
      fprintf(stderr, "[FATAL][%s] Failed to create child socket ... exiting\n",
              date_str(now_str));
      return 1;
    }
    printf("[DEBUG][%s] Waiting for instructions ...\n", date_str(now_str));

    // communicate with client
    chat(child_socket, params);

  } catch (std::exception &e) {
    fprintf(stderr, "[ERROR][%s] Failed creating deamon\n", date_str(now_str));
    fprintf(stderr, "[FATAL][%s] ... exiting\n", date_str(now_str));
  }

  // shutdown system
  system_shutdown();

  return 0;
}
