#!/bin/sh
set -eu

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libpcpnatpmp; then
    if cc pcp-example.c -o pcp-example -O0 -g $(pkg-config --cflags --libs libpcpnatpmp) 2>/dev/null; then
        exit 0
    fi
fi

cc pcp-example.c -o pcp-example -O0 -g -I ../lib/include -L ../lib/.libs -lpcpnatpmp
