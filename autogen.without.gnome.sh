#!/bin/bash
test ! -e INSTALL &&touch INSTALL
test ! -e NEWS &&touch NEWS
test ! -e README &&touch README
test ! -e AUTHORS &&touch AUTHORS
test ! -e ChangeLog &&touch ChangeLog
test ! -e COPYING &&touch COPYING

test ! -e configure.ac && autoscan && echo AM_INIT_AUTOMAKE >>configure.scan && nano configure.scan && mv configure.scan configure.ac 

libtoolize --copy --force &&
glib-gettextize -f &&
intltoolize --force &&
aclocal --force -I ./m4 &&
autoconf &&
autoheader &&
automake --add-missing &&
./configure &&
make
