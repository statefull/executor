#!/bin/bash
libtoolize
aclocal
autoheader
autoconf
automake --add-missing
./configure; make
