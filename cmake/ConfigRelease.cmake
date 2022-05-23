include_guard(GLOBAL)

if (APPLE)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -mtune=native")
elseif (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if (${CMAKE_CXX_COMPILER} MATCHES "-[0-9]+$")
            string(REGEX REPLACE [[.*clang\+\+(-[0-9]+)$]] [[lld\1]] LD ${CMAKE_CXX_COMPILER})
            set(STATIC_LINK_FLAG "-static -fuse-ld=${LD}")
        else ()
            set(STATIC_LINK_FLAG "-static -fuse-ld=lld")
        endif ()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    elseif (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # ref: https://gist.github.com/ken-matsui/f6c736ea9623cc15e0a1e8912cae5718
        # I'm not sure why, but this fails on GCC with release builds
        set(STATIC_LINK_FLAG "-static") # ref: https://stackoverflow.com/a/3801032
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif ()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif ()
if (UNIX AND NOT APPLE)
    set(Boost_USE_STATIC_LIBS ON)
endif ()
