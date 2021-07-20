#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>
#include <cppfits.hpp>

constexpr int MAX_PIXELS_IN_DIM = 2048;

int get_single_scan(const AndorParameters *params, int xpixels, int ypixels, at_32* img_buffer) noexcept;

int get_acquisition(const AndorParameters *params) noexcept {
  
  char buf[32] = {'\0'}; /* vuffer for datetime string */

  if (params->read_out_mode_ != ReadOutMode::Image) {
    fprintf(stderr, "[ERROR][%s] Can only acquire images in Image ReadOut Mode! (traceback: %s)", date_str(buf), __func__);
    return 1;
  }

  int xpixels, ypixels;
  if (GetDetector(&xpixels, &ypixels) != DRV_SUCCESS) {
    fprintf(stderr, "[ERROR][%s] Failed getting detector size! (traceback: %s)", date_str(buf), __func__);
    return 1;
  }
  if (xpixels != MAX_PIXELS_IN_DIM || ypixels != MAX_PIXELS_IN_DIM) {
    fprintf(stderr, "[ERROR][%s] Detector returned erronuous image size (%6dx%6d)! (traceback: %s)", date_str(buf), xpixels, ypixels, __func__);
    return 1;
  }

  int xnumpixels = params->image_hend_ - params->image_hstart_ + 1;
  int ynumpixels = params->image_vend_ - params->image_vstart_ + 1;
  xnumpixels /= params->image_hbin_;
  ynumpixels /= params->image_vbin_;
  if ((xnumpixels<1 || xnumpixels>MAX_PIXELS_IN_DIM) || (ynumpixels<1 || ynumpixels>MAX_PIXELS_IN_DIM)) {
    fprintf(stderr, "[ERROR][%s] Computed image size outside valid rage! (traceback: %s)", date_str(buf), __func__);
    return 1;
  }

  if (setup_acquisition_mode(params)) {
    fprintf(stderr, "[ERROR][%s] Failed setting up acquisition mode! (traceback: %s)", date_str(buf), __func__);
    return 1;
  }

  long image_pixels = xnumpixels * ynumpixels;
  at_32 *img_buffer = new at_32[image_pixels];

  int acq_status = 0;
  if (params->acquisition_mode_ == AcquisitionMode::SingleScan) {
    acq_status = get_single_scan(params, xnumpixels, ynumpixels, img_buffer);
  }

  if (acq_status) {
    fprintf(stderr, "[ERROR][%s] Failed acquiring image! (traceback: %s)", date_str(buf), __func__);
  }

  delete[] img_buffer;

  return acq_status;
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
    
    unsigned int error = GetAcquiredData(img_buffer, static_cast<unsigned long>(xpixels * ypixels));
    if (error != DRV_SUCCESS) {
      fprintf(stderr, "[ERROR][%s] Failed to get acquired data! Aborting acquisition (traceback: %s)\n", date_str(buf), __func__);
      AbortAcquisition();
      return 1;
    }
    
    if (get_next_fits_filename(params, fits_filename)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 1;
    }
    
    printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...",
           date_str(buf), fits_filename);

    
    // for (int i = 0; i < xpixels * ypixels; i++)
    //  fout << imageData[i] << std::endl;
    /*if (SaveAsFITS(fits_file, 3) != DRV_SUCCESS) 
      fprintf(stderr, "\n[ERROR][%s] Failed to save image to fits format!\n",
              date_str()); */
    
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