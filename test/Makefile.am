noinst_PROGRAMS = testClientSocket \
  testServerSocket \
  testCmdParser \
  testFitsFilename

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
	-DDEBUG

# testReadParameterFile_SOURCES   = test_read_parameter_file.cpp
# testReadParameterFile_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
# testReadParameterFile_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
# 
testClientSocket_SOURCES   = test_client_socket.cpp
testClientSocket_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
#testClientSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
testClientSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -landor -lcfitsio -lbz2 -lpthread -lm

testServerSocket_SOURCES   = test_server_socket.cpp
testServerSocket_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
#testServerSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
testServerSocket_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -landor -lcfitsio -lbz2 -lpthread -lm

testCmdParser_SOURCES   = test_cmd_parser.cpp
testCmdParser_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
#testCmdParser_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
testCmdParser_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -landor -lcfitsio -lbz2 -lm

testFitsFilename_SOURCES   = test_fits_filename.cpp
testFitsFilename_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
testFitsFilename_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -landor -lcfitsio -lbz2 -lm
#
#testFitsFilenameNext_SOURCES   = test_fits_filename.cpp
#testFitsFilenameNext_CXXFLAGS  = $(MCXXFLAGS) -I$(top_srcdir)/src #-L$(top_srcdir)/src
#testFitsFilenameNext_LDADD     = $(top_srcdir)/src/libhelmosandor2k.la -lcfitsio -lm
