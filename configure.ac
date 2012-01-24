dnl Process this file with autoconf to produce a configure script.

AC_INIT(atmd_server, 2.5)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AC_ISC_POSIX
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC

LIBCURL_CHECK_CONFIG([yes], [7])
AC_SUBST(CURLLIBS)

PKG_CHECK_MODULES([PCI], [libpci >= 3.0], , [AC_CHECK_LIB(pci, pci_scan_bus)])

PKG_CHECK_MODULES([PCRECPP], [libpcrecpp >= 7])


dnl Search for XENOMAI

XENO_CONFIG=`which xeno-config 2>/dev/null`
if test "${XENO_CONFIG}" = "" || test \! -x ${XENO_CONFIG} || test \! -f ${XENO_CONFIG}; then
    AC_MSG_ERROR([*** Xenomai configuration tool not found: ${XENO_CONFIG}])
fi

AC_MSG_CHECKING([for Xenomai version])
XENO_VERSION="`${XENO_CONFIG} --version`"

case "$XENO_VERSION" in
    2.5*|2.6*)
        AC_MSG_RESULT([${XENO_VERSION}])
        ;;
    *)
        AC_MSG_ERROR([*** Unsupported Xenomai version $XENO_VERSION in $XENO_DIR])
        ;;
esac

XENO_LIBS="`${XENO_CONFIG} --skin native --ldflags`"
XENO_CFLAGS="`${XENO_CONFIG} --skin native --cflags`"
AC_SUBST(XENO_LIBS)
AC_SUBST(XENO_CFLAGS)

AC_OUTPUT([Makefile src/Makefile])
