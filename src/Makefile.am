##
##  The library will be called: (lib)"aristarchos".
## ------------------------------------------------
##
lib_LTLIBRARIES = libaristarchos.la

##
##  Only use the following flags for debuging purposes.
## ----------------------------------------------------
##
libaristarchos_la_CXXFLAGS = \
	-std=c++17 \
	-g \
	-pg \
	-Wall \
	-Wextra \
	-Werror \
	-pedantic \
	-W \
	-Wshadow \
	-Winline \
	-Wdisabled-optimization \
	-march=native \
	-DDEBUG \
	-DSOCKET_LOGGER

##
##  Header files (distributed) installed in /$(includedir)/$(package name).
## ------------------------------------------------------------------------
##
pkginclude_HEADERS = \
        ccd_params.hpp \
        fits_image.hpp \
        parameter_file.hpp \
	fits_header_buffer.hpp \
        cpp_socket.hpp \
	fits_filename.hpp

##
##  Source files (distributed).
## ----------------------------
##
dist_libaristarchos_la_SOURCES = \
        ccd_params.cpp \
        fitsimage.cpp \
        parameter_file.cpp \
	cpp_socket.cpp \
	fits_filename.cpp
