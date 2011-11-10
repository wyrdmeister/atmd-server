#!/bin/sh

# Script to setup autoconf

aclocal
autoconf
automake --add-missing
autoreconf
