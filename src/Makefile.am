##
##  The library will be called: (lib)"helmosandor2k".
## ------------------------------------------------
##
lib_LTLIBRARIES = libhelmosandor2k.la

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
	-DDEBUG \
	-DSOCKET_LOGGER

##
##  Header files (distributed) installed in /$(includedir)/$(package name).
## ------------------------------------------------------------------------
##
pkginclude_HEADERS = \
        cpp_socket.hpp

##
##  Source files (distributed).
## ----------------------------
##
dist_libhelmosandor2k_la_SOURCES = \
	cpp_socket.cpp
