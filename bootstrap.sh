#!/bin/sh

# Script to setup autoconf
touch stamp-h
aclocal
autoconf
automake --add-missing
autoreconf
