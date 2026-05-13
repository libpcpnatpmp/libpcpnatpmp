cmake_minimum_required(VERSION 3.14)

project (pcpnatpmp-example C)

find_package (pcpnatpmp CONFIG REQUIRED)

add_executable (pcp-example pcp-example.c)
target_link_libraries (pcp-example PRIVATE pcpnatpmp::pcpnatpmp)
