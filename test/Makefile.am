noinst_PROGRAMS = testClientSocket \
  testServerSocket

MCXXFLAGS = \
	-std=c++17 \
	-g \
	-pg \
	-Wno-unused-variable \
	-Wall \
	-Wextra \
	-Werror \
	-pedantic \
	-W \
	-Wshadow \
	-Wdisabled-optimization \
	-DDEBUG \
	-DSOCKET_LOGGER

# testReadParameterFile_SOURCES   = test_read_parameter_file.cpp
# testReadParameterFile_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
# testReadParameterFile_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
# 
testClientSocket_SOURCES   = test_client_socket.cpp
testClientSocket_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
testClientSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm

testServerSocket_SOURCES   = test_server_socket.cpp
testServerSocket_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
testServerSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
#
#testFitsFilenameNext_SOURCES   = test_fits_filename.cpp
#testFitsFilenameNext_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
#testFitsFilenameNext_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm