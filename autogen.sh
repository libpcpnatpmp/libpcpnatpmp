#!/bin/bash

rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh

autoconf_minor_version=`autoconf --version | grep -Eo "^autoconf.* ([0-9]+\.[0-9]+)" | grep -Eo "([0-9]+)$"`
autoconf_major_version=`autoconf --version | grep -Eo "^autoconf.* ([0-9]+\.[0-9]+)" | grep -Eo " ([0-9]+)"`

if ! [ $autoconf_minor_version -gt "61" -a $autoconf_major_version -ge "2" ]; then
[ -f configure.ac.orig ] || mv configure.ac configure.ac.orig
grep -v -e AM_PROG_AR configure.ac.orig >configure.ac
sed -i 's/AM_SILENT_RULES.*/AC_MSG_NOTICE\(\[\]\)/' configure.ac
sed -i 's/AC_PREREQ.*/AC_PREREQ\(\[2\.61\]\)/' configure.ac
sed -i 's/color-tests subdir-objects//' configure.ac
fi

mkdir -p m4
echo "Running aclocal..." ; aclocal $ACLOCAL_FLAGS -I m4 || exit 1
echo "Running autoheader..." ; autoheader || exit 1
echo "Running autoconf..." ; autoconf || exit 1
echo "Running libtoolize..." ; (libtoolize --copy --automake || glibtoolize --automake) || exit 1
echo "Running automake..." ; automake --add-missing --copy --gnu || exit 1

[ -f configure.ac.orig ] && mv configure.ac.orig configure.ac

exit 0
