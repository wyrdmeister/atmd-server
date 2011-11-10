dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(atmd_server, 2.0)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_HEADERS([config.h])

AC_ISC_POSIX
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC

PKG_CHECK_MODULES([PCI], [libpci >= 3.0], , [AC_CHECK_LIB(pci, pci_scan_bus)])

PKG_CHECK_MODULES([PCRECPP], [libpcrecpp >= 7])


AC_CHECK_LIB(pthread, pthread_create, [PTHREAD_LIBS=-lpthread], [AC_MSG_ERROR([Cannot find libpthread.])])
AC_SUBST([PTHREAD_LIBS])

AC_OUTPUT([
Makefile
src/Makefile
])

