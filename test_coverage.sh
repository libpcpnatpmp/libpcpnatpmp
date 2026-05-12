#!/bin/sh

rm test_coverage.tmp -rf
mkdir test_coverage.tmp
#[ -f configure.ac.orig ] || cp configure.ac configure.ac.orig
#sed -i 's/subdir-objects//' configure.ac
./autogen.sh
cd test_coverage.tmp
CPPFLAGS="-DNDEBUG -DPCP_MAX_LOG_LEVEL=5" CFLAGS="-O0 -g" ../configure --enable-gcov
make check
rm cli-client/pcpnatpmpc-pcpnatpmpc.gcda
lcov -c --directory . --output-file info && genhtml -o report/ info && cd .. && rm -rf test_coverage && mv test_coverage.tmp test_coverage

URL=test_coverage/report/index.html
if [ -n "$BROWSER" ] && [ -x "$BROWSER" ]; then
    exec "$BROWSER" "$URL"
fi
path=$(command -v xdg-open || command -v gnome-open || command -v open) && exec "$path" "$URL"
