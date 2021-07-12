#ifndef __CPP_CFITSIO_ANDOR2K_H__
#define __CPP_CFITSIO_ANDOR2K_H__

#include "fitsio.h"
#include <cstdint>

namespace fits_details {
constexpr int ANDOR2K_MAX_XPIXELS = 2048;
constexpr int ANDOR2K_MAX_YPIXELS = 2048;

/// see https://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/c_user/node20.html
template<typename T> struct cfitsio_bitpix {};
template<> struct cfitsio_bitpix<int8_t> { 
    static constexpr int bitpix = 8;
    static constexpr int bscale = 1;
    static constexpr int bzero = -128;
};
template<> struct cfitsio_bitpix<uint16_t> { 
    static constexpr int bitpix = 16;
    static constexpr int bscale = 1;
    static constexpr int bzero = 32768;
};
template<> struct cfitsio_bitpix<uint32_t> { 
    static constexpr int bitpix = 32;
    static constexpr int bscale = 1;
    static constexpr int bzero = 2147483648;
};
template<> struct cfitsio_bitpix<uint64_t> { 
    static constexpr int bitpix = 64;
    static constexpr int bscale = 1;
    static constexpr int bzero = 9223372036854775808;
};
template<typename T> struct cfitsio_type {};
template<> struct cfitsio_type<signed short> { static constexpr int type = TSHORT; };
template<> struct cfitsio_type<unsigned short> { static constexpr int type = TUSHORT; };
template<> struct cfitsio_type<signed int> { static constexpr int type = TINT; };
template<> struct cfitsio_type<unsigned int> { static constexpr int type = TUINT; };
template<> struct cfitsio_type<signed long> { static constexpr int type = TLONG; };
template<> struct cfitsio_type<unsigned long> { static constexpr int type = TULONG; };
template<> struct cfitsio_type<float> { static constexpr int type = TFLOAT; };
template<> struct cfitsio_type<double> { static constexpr int type = TDOUBLE; };
}// fits_details

///
/// @warning This class can only handle 2-dimensional images
template <typename T>
class FitsImage {
private:
    fitsfile *fptr; /* pointer to fits file; defined in fitsio */
    // T data[ANDOR2K_MAX_XPIXELS * ANDOR2K_MAX_YPIXELS];
    char filename[256];
    int xpixels, ypixels;
    int bitpix = fits_details::cfitsio_bitpix<T>::bitpix;
public:
    FitsImage(const char *fn, int width, int height) noexcept :
        xpixels(width), ypixels(height) {
            std::strcpy(filename, fn);
            int status = 0;
            if (fits_create_file(&fptr, filename, &status))
                fits_report_error(stderr, status);
        }
    
    template<typename S>
    int write(S* image) noexcept {
        using fits_details::cfitsio_type;
        int status = 0;
        
        long naxes[] = {ypixels, xpixels};
        if (fits_create_img(fptr, bitpix, 2, naxes, &status))
            fits_report_error(stderr, status);
        
        if (fits_write_img(fptr, cfitsio_type<S>::type, 1, xpixels*ypixels, image, &status))
            fits_report_error(stderr, status);
        
        return status;
    }

    int close() noexcept {
        int status = 0;
        if (fits_close_file(fptr, &status)) 
            fits_report_error(stderr, status);
        return status;
    }

    template<typename K>
    int update_key(char* keyname, K* value, char* comment) noexcept {
        using fits_details::cfitsio_type;
        int status = 0;
        if (fits_update_key(fptr, cfitsio_type<K>::type, keyname, value, comment, &status))
            fits_report_error(stderr, status);
        return status;
    }
};

#endif