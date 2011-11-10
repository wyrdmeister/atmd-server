## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = atmd_server

atmd_server_SOURCES = \
	common.h      \
	atmd_hardware.cpp      \
	atmd_hardware.h      \
	main.cpp      \
	atmd_timings.cpp      \
	atmd_timings.h      \
	atmd_measure_direct.cpp      \
	atmd_measure_burst.cpp	\
	atmd_measure.cpp      \
	atmd_measure.h      \
	atmd_network.cpp      \
	atmd_network.h

atmd_server_LDFLAGS = $(PCI_LIBS) $(PCRECPP_LIBS) $(PTHREAD_LIBS)

atmd_server_LDADD = 

