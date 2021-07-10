bin_PROGRAMS = dandor2kd

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
	-DDEBUG

dandor2kd_SOURCES = andor2kd.cpp
dandor2kd_CXXFLAGS = $(MXXFLAGS) -I$(top_srcdir)/src
dandor2kd_LDADD = $(top_srcdir)/src/libhelmosandor2k.la -landor
