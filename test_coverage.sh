#!/bin/sh

rm test_coverage.tmp -rf
mkdir test_coverage.tmp
cd test_coverage.tmp
cmake -S .. -B . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-DNDEBUG -DPCP_MAX_LOG_LEVEL=5 -O0 -g" \
    -DENABLE_GCOV=ON
cmake --build .
ctest --output-on-failure

# CLI binary is not executed during tests, so skip it in coverage collection.
rm -f cli-client/CMakeFiles/pcpnatpmpc.dir/*.gcda

lcov -c --directory . --output-file info
lcov --ignore-errors unused --remove info \
    '/usr/*' \
    '*/tests/*' \
    '*/test-server/*' \
    '*/cli-client/*' \
    '*/CMakeFiles/*' \
    --output-file info.filtered
genhtml -o report/ info.filtered && cd .. && rm -rf test_coverage && mv test_coverage.tmp test_coverage

URL=test_coverage/report/index.html
if [ -n "$BROWSER" ] && [ -x "$BROWSER" ]; then
    exec "$BROWSER" "$URL"
fi
path=$(command -v xdg-open || command -v gnome-open || command -v open) && exec "$path" "$URL"
