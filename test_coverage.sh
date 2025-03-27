#/bin/sh

rm test_coverage.tmp -rf
mkdir test_coverage.tmp
#[ -f configure.ac.orig ] || cp configure.ac configure.ac.orig
#sed -i 's/subdir-objects//' configure.ac
./autogen.sh
cd test_coverage.tmp
CPPFLAGS="-DNDEBUG -DPCP_MAX_LOG_LEVEL=5" CFLAGS="-O0 -g" ../configure --enable-gcov
make check
rm cli-client/pcp-pcp_app.gcda
lcov -c --directory . --output-file info && genhtml -o report/ info && cd .. && rm -rf test_coverage && mv test_coverage.tmp test_coverage

URL=test_coverage/report/index.html
[[ -x $BROWSER ]] && exec "$BROWSER" "$URL"
path=$(which xdg-open || which gnome-open || which open) && exec "$path" "$URL"

