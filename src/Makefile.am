## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

AM_CFLAGS = -Wall -g

bin_PROGRAMS = atmd_server atmd_bntest

atmd_server_SOURCES = \
	common.h      \
	atmd_hardware.cpp      \
	atmd_hardware.h      \
	main.cpp      \
	atmd_timings.cpp      \
	atmd_timings.h      \
	atmd_measure_direct.cpp      \
	atmd_measure_direct.h      \
	atmd_measure.cpp      \
	atmd_measure.h      \
	atmd_network.cpp      \
	atmd_network.h      \
	MatFile.cpp

atmd_bntest_SOURCES = atmd_bntest.cpp

atmd_server_LDFLAGS = $(LDFLAGS) $(PCI_LIBS) $(PCRECPP_LIBS) $(LIBCURL) $(XENO_LIBS)
atmd_server_CFLAGS = $(CFLAGS) $(XENO_CFLAGS)
atmd_server_CPPFLAGS = $(CPPFLAGS) $(XENO_CFLAGS) $(LIBCURL_CPPFLAGS)

atmd_bntest_LDFLAGS = $(LDFLAGS)
atmd_bntest_CFLAGS = $(CFLAGS)
atmd_bntest_CPPFLAGS = $(CPPFLAGS)
