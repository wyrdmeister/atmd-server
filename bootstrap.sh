#!/bin/sh

# Script to setup autoconf
autoheader
touch stamp-h
aclocal
autoconf
automake --add-missing
autoreconf
