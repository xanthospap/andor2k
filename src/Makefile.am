##
##  The library will be called: (lib)"helmosandor2k".
## ------------------------------------------------
##
lib_LTLIBRARIES = libhelmosandor2k.la
#bin_PROGRAMS = andor2k \
#	andor2kd

##
##  Only use the following flags for debuging purposes.
## ----------------------------------------------------
##
libhelmosandor2k_la_CXXFLAGS = \
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
	-DDEBUG

MCXXFLAGS = \
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
        cpp_socket.hpp \
		andor2k.hpp \
		cppfits.hpp \
		andor2kd.hpp

##
##  Source files (distributed).
## ----------------------------
##
dist_libhelmosandor2k_la_SOURCES = \
        andor2k.cpp \
	cpp_socket.cpp \
	cmd_parser.cpp \
	andor_tools.cpp \
	set_readout_mode.cpp \
	set_acquisition_mode.cpp \
	cooling.cpp \
	fits_filenames.cpp \
	sys_shutdown.cpp \
	usage.cpp \
	get_acquisition.cpp \
	set_acquisition.cpp \
	resolve_image_parameters.cpp

#andor2k_SOURCES = andor2k.cpp
#andor2k_CXXFLAGS = $(MXXFLAGS) -I$(top_srcdir)/src
#andor2k_LDADD = $(top_srcdir)/src/libhelmosandor2k.la -landor
#
#andor2kd_SOURCES = andor2kd.cpp
#andor2kd_CXXFLAGS = $(MXXFLAGS) -I$(top_srcdir)/src
#andor2kd_LDADD = $(top_srcdir)/src/libhelmosandor2k.la -landor
