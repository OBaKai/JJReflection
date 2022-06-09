cmake_minimum_required(VERSION 3.4.1)

add_library(jj-reflection
             SHARED
             main.cpp
            )

find_library(log-lib
              log )

target_link_libraries(jj-reflection
                       ${log-lib} )