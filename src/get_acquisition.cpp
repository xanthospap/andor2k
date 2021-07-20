#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <cppfits.hpp>

using namespace std::chrono_literals;

constexpr int MAX_PIXELS_IN_DIM = 2048;

int get_single_scan(const AndorParameters *params, int xpixels, int ypixels, at_32* img_buffer) noexcept;
int get_rta_scan(const AndorParameters *params, int xpixels, int ypixels, at_32* img_buffer) noexcept;

int get_acquisition(const AndorParameters *params) noexcept {
  
  char buf[32] = {'\0'}; /* buffer for datetime string */

  /* check and set read-out mode */
  if (params->read_out_mode_ != ReadOutMode::Image) {
    fprintf(stderr, "[ERROR][%s] Can only acquire images in Image ReadOut Mode! (traceback: %s)\n", date_str(buf), __func__);
    return 1;
  }
  if (setup_read_out_mode(params)) {
    fprintf(stderr, "[ERROR][%s] Failed to set read mode! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }
  
  /* set acquisition mode (this will also set the exposure time) */
  if (setup_acquisition_mode(params)) {
    fprintf(stderr, "[ERROR][%s] Failed to set acquisition mode! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }
  
  /* initialize shutter */
  unsigned int error =
      SetShutter(1, ShutterMode2int(params->shutter_mode_),
                 params->shutter_closing_time_, params->shutter_opening_time_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed to initialize shutter! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  int xpixels, ypixels;
  if (GetDetector(&xpixels, &ypixels) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed getting detector size! (traceback: %s)\n", date_str(buf), __func__);
    return 1;
  }
  if (xpixels != MAX_PIXELS_IN_DIM || ypixels != MAX_PIXELS_IN_DIM) {
    fprintf(stderr, "[ERROR][%s] Detector returned erronuous image size (%6dx%6d)! (traceback: %s)\n", date_str(buf), xpixels, ypixels, __func__);
    return 1;
  }

  int xnumpixels = params->image_hend_ - params->image_hstart_ + 1;
  int ynumpixels = params->image_vend_ - params->image_vstart_ + 1;
  xnumpixels /= params->image_hbin_;
  ynumpixels /= params->image_vbin_;
  if ((xnumpixels<1 || xnumpixels>MAX_PIXELS_IN_DIM) || (ynumpixels<1 || ynumpixels>MAX_PIXELS_IN_DIM)) {
    fprintf(stderr, "[ERROR][%s] Computed image size outside valid rage! (traceback: %s)\n", date_str(buf), __func__);
    return 1;
  }

  printf("[DEBUG][%s] Computed pixels = %5dx%5d\n", date_str(buf), xnumpixels, ynumpixels);
  printf("[DEBUG][%s] Detector pixels = %5dx%5d\n", date_str(buf), xpixels, ypixels);
  long image_pixels = xnumpixels * ynumpixels;
  at_32 *img_buffer = new at_32[image_pixels];

  int acq_status = 0;
  if (params->acquisition_mode_ == AcquisitionMode::SingleScan) {
    acq_status = get_single_scan(params, xnumpixels, ynumpixels, img_buffer);
  } else if (params->acquisition_mode_ == AcquisitionMode::RunTillAbort) {
    acq_status = get_rta_scan(params, xnumpixels, ynumpixels, img_buffer);
  }

  if (acq_status) {
    fprintf(stderr, "[ERROR][%s] Failed acquiring image! (traceback: %s)\n", date_str(buf), __func__);
  }

  delete[] img_buffer;

  return acq_status;
}

int get_rta_scan(const AndorParameters *params, int xpixels, int ypixels, at_32* img_buffer) noexcept {
    char buf[32] = {'\0'}; /* buffer for datetime string */
    char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */
    
    printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf), params->num_images_);
    StartAcquisition();

    unsigned int error;
    int images_remaining = params->num_images_;
    int buffer_images_remaining = 0;
    int buffer_images_retrieved = 0;
    int status;
    at_32 series = 0, first, last;
    
    GetTotalNumberImagesAcquired(&series);
    printf("-----> GetTotalNumberImagesAcquired = %d\n", series);
    if (GetNumberNewImages(&first, &last) == DRV_SUCCESS) {
      printf("-----> GetNumberNewImages: first %d, last %d\n", first, last);
    }
    
    GetStatus(&status);
    auto acq_start_t = std::chrono::high_resolution_clock::now();
    while ( (status == DRV_ACQUIRING && images_remaining > 0) || buffer_images_remaining > 0 ) {
      auto nowt1 = std::chrono::high_resolution_clock::now();
      auto elapsed_time_sec1 = std::chrono::duration_cast<std::chrono::seconds>(nowt1-acq_start_t);
      printf("[DEBUG][%s] Ellapsed time acquiring %10ld seconds; current series %d buf. images retrieved %d buf images remaining %d\n", date_str(buf), elapsed_time_sec1.count(), series, buffer_images_retrieved, buffer_images_remaining);
      if (images_remaining == 0) error = AbortAcquisition();

      GetTotalNumberImagesAcquired(&series);

      if (GetNumberNewImages(&first, &last) == DRV_SUCCESS) {
        printf("-----> first and last in buffer: %d and %d\n", first, last);
        buffer_images_remaining = last - first;
        images_remaining = params->num_images_ - series;

        error = GetOldestImage(img_buffer, xpixels*ypixels);

        if (error == DRV_P2INVALID || error == DRV_P1INVALID) {
          fprintf(stderr, "[ERROR][%s] Acquisition error, nr #%d (traceback: %s)\n", date_str(buf), error, __func__);
          fprintf(stderr, "[ERROR][%s] Aborting acquisition.\n", date_str(buf));
          AbortAcquisition();
        }

        if (error==DRV_SUCCESS) {
          ++buffer_images_retrieved;
          get_next_fits_filename(params, fits_filename);
          FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
          if ( fits.write<at_32>(img_buffer) ) {
            fprintf(stderr, "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n", date_str(buf), __func__);
            AbortAcquisition();
            // return 15;
          } else {
            printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf), fits_filename);
          }
          fits.update_key<int>("NXAXIS1", &xpixels, "width");
          fits.update_key<int>("NXAXIS2", &ypixels, "height");
          fits.close();
        }
      }

      // abort after full number of images taken
      if (series >= params->num_images_) {
        printf("[DEBUG][%s] Succesefully acquired all images (aka %d)\n", date_str(buf), series);
        AbortAcquisition();   
      }

      GetStatus(&status);
      auto nowt = std::chrono::high_resolution_clock::now();
      auto elapsed_time_sec = std::chrono::duration_cast<std::chrono::seconds>(nowt-acq_start_t);
      /*if (elapsed_time_sec > 3 * 60 * 1000) {
        fprintf(stderr, "[ERROR][%s] Aborting acquisition cause it took too much time! (traceback: %s)\n", date_str(buf), __func__);
        AbortAcquisition();
        GetStatus(&status);
      }*/
      GetStatus(&status);
      printf("[DEBUG][%s] Ellapsed time acquiring %10ld seconds; current series %d buf. images retrieved %d buf images remaining %d\n", date_str(buf), elapsed_time_sec.count(), series, buffer_images_retrieved, buffer_images_remaining);
      std::this_thread::sleep_for(1000ms);
    }

    return 0;
}

int get_single_scan(const AndorParameters *params, int xpixels, int ypixels, at_32* img_buffer) noexcept {
  
    char buf[32] = {'\0'}; /* buffer for datetime string */
    char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */
    
    printf("[DEBUG][%s] Starting image acquisition ...\n", date_str(buf));
    StartAcquisition();
    
    // Loop until acquisition finished
    int status;
    GetStatus(&status);
    
    while (status == DRV_ACQUIRING)
      GetStatus(&status);
    
    unsigned int error = GetAcquiredData(img_buffer, xpixels * ypixels);
    if (error != DRV_SUCCESS) {
      fprintf(stderr, "[ERROR][%s] Failed to get acquired data! Aborting acquisition (traceback: %s)\n", date_str(buf), __func__);
      AbortAcquisition();
      if (error == DRV_ACQUIRING)
        fprintf(stderr, "[ERROR][%s] Error Message: Acquisition in progress (traceback: %s)\n", date_str(buf), __func__);
      else if (error == DRV_ERROR_ACK)
        fprintf(stderr, "[ERROR][%s] Error Message: Unable to communicate with card (traceback: %s)\n", date_str(buf), __func__);
      else if (error == DRV_P1INVALID)
        fprintf(stderr, "[ERROR][%s] Error Message: Invalid pointer (traceback: %s)\n", date_str(buf), __func__);
      else if (error == DRV_P2INVALID)
        fprintf(stderr, "[ERROR][%s] Error Message: Array size is incorrect (traceback: %s)\n", date_str(buf), __func__);
      else if (error == DRV_NO_NEW_DATA)
        fprintf(stderr, "[ERROR][%s] Error Message: No acquisition has taken place (traceback: %s)\n", date_str(buf), __func__);
      else
        fprintf(stderr, "[ERROR][%s] Error Message: Undocumented error (traceback: %s)\n", date_str(buf), __func__);
      return 1;
    }
    
    if (get_next_fits_filename(params, fits_filename)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 1;
    }
    
    printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...\n",
           date_str(buf), fits_filename);

    
    /*
    if (SaveAsFITS(fits_filename, 3) != DRV_SUCCESS) 
      fprintf(stderr, "\n[ERROR][%s] Failed to save image to fits format! (traceback: %s)\n",
              date_str(buf), __func__);
    */
    FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
    if ( fits.write<at_32>(img_buffer) ) {
      fprintf(stderr, "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n", date_str(buf), __func__);
      return 15;
    } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf), fits_filename);
    }
    fits.update_key<int>("NXAXIS1", &xpixels, "width");
    fits.update_key<int>("NXAXIS2", &ypixels, "height");
    fits.close();

  return 0;
}
