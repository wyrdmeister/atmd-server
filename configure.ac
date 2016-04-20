dnl Process this file with autoconf to produce a configure script.

AC_INIT(atmd_server, 3.0)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

CPPFLAGS="-Wall"
CXXFLAGS="$CPPFLAGS"

AC_ISC_POSIX
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC

dnl Enable tango
AC_ARG_ENABLE(tango, [  --enable-tango  Enable TANGO support], [en_tango=yes], [])

dnl Enable debug
AC_ARG_ENABLE(debug, [  --enable-debug  Enable debug code], [en_debug=yes], [])

dnl Define targets
TARGETS="term_rtdev"

dnl Enable compilation of atmd_server
AC_ARG_ENABLE(server, [  --enable-server  Enable build of server program], [en_server=yes], [])

dnl Enable compilation of atmd_server
AC_ARG_ENABLE(agent, [  --enable-agent  Enable build of agent program], [en_agent=yes], [])

if test yes != "$en_agent" && test yes != "$en_server"; then
   AC_MSG_ERROR([*** You should at least enable build of server or agent. ***])
fi

dnl Get default CPPFLAGS
if test yes = "$en_debug"; then
  CPPFLAGS="$CPPFLAGS -g -O0 -DDEBUG"
else
  CPPFLAGS="$CPPFLAGS -O2"
fi

dnl Check for common libraries
dnl Check for libpcre++
PKG_CHECK_MODULES([PCRECPP], [libpcrecpp >= 7])

dnl Search for XENOMAI
XENO_CONFIG=`which xeno-config 2>/dev/null`
if test "${XENO_CONFIG}" = "" || test \! -x ${XENO_CONFIG} || test \! -f ${XENO_CONFIG}; then
    AC_MSG_ERROR([*** Xenomai configuration tool not found: ${XENO_CONFIG}])
fi

AC_MSG_CHECKING([for Xenomai version])
XENO_VERSION="`${XENO_CONFIG} --version`"

case "$XENO_VERSION" in
    2.6*)
        AC_MSG_RESULT([${XENO_VERSION}])
        ;;
    *)
        AC_MSG_ERROR([*** Unsupported Xenomai version $XENO_VERSION in $XENO_DIR])
        ;;
esac

XENO_LIBS="`${XENO_CONFIG} --skin rtdm --ldflags --skin native --ldflags | tr '\n' ' '`"
AC_SUBST([XENO_LIBS])
CPPFLAGS="$CPPFLAGS `${XENO_CONFIG} --skin native --cflags | sed 's/-Werror-implicit-function-declaration//g' | tr '\n' ' '`"

dnl Search for RTNET
RTNET_DIR="`which rtnet | sed -e 's/\/sbin\/rtnet//'`"
if test \! -d "${RTNET_DIR}/include"; then
    AC_MSG_ERROR([*** RTnet install directory not found! ***])
fi
CPPFLAGS="$CPPFLAGS -I${RTNET_DIR}/include"

dnl If TANGO is enabled
if test yes = "$en_tango"; then
  if test yes = "$en_server"; then
    PKG_CHECK_MODULES([TANGO], [tango >= 7])
  fi
  CPPFLAGS="$CPPFLAGS -DEN_TANGO"
fi

dnl If we build server...
if test yes = "$en_server"; then
  TARGETS="$TARGETS atmd_server"

  dnl Check for libcurl
  LIBCURL_CHECK_CONFIG(, [7])
  AC_SUBST(CURLLIBS)
fi

dnl If we build agent...
if test yes = "$en_agent"; then
  TARGETS="$TARGETS atmd_agent"

  dnl Check for libpci
  PKG_CHECK_MODULES([PCI], [libpci >= 3.0], [], [AC_CHECK_LIB(pci, pci_scan_bus)])
  AC_CHECK_HEADERS([pci/pci.h], [], [AC_MSG_ERROR([Cannot find pci.h - Maybe you need to install libpci-dev])])
fi

AC_SUBST([TARGETS])
AC_MSG_NOTICE([ => Building targets: ${TARGETS}])

AC_OUTPUT([Makefile src/Makefile])
