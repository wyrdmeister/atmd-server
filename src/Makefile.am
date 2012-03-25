## Process this file with automake to produce Makefile.in

bin_PROGRAMS = $(TARGETS)
EXTRA_PROGRAMS = atmd_server atmd_agent

atmd_server_SOURCES = \
	atmd_server.cpp \
	atmd_netagent.cpp \
	atmd_virtualboard.cpp \
	atmd_config.cpp \
	atmd_rtnet.cpp \
	atmd_timings.cpp \
	atmd_network.cpp \
	atmd_measure.cpp \
	atmd_rtqueue.cpp \
	atmd_rtcomm.cpp \
	MatFile.cpp \
	std_fileno.cpp

atmd_agent_SOURCES = \
	atmd_agent.cpp \
	atmd_agentmeasure.cpp \
	atmd_hardware.cpp \
	atmd_netagent.cpp \
	atmd_config.cpp \
	atmd_rtnet.cpp \
	atmd_rtcomm.cpp

atmd_server_LDADD = $(XENO_LIBS) $(PCRECPP_LIBS) $(LIBCURL) $(XENO_LIBS) $(TANGO_LIBS)
atmd_server_CPPFLAGS = $(CPPFLAGS) $(LIBCURL_CPPFLAGS) $(TANGO_CFLAGS) -DATMD_SERVER

atmd_agent_LDADD = $(XENO_LIBS) $(PCRECPP_LIBS) $(PCI_LIBS) $(XENO_LIBS)
atmd_agent_CPPFLAGS = $(CPPFLAGS) -DATMD_AGENT
