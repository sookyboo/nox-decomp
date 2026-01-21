# CMakeLists.txt
#if (MSVC)
#    add_compile_options ("$<$<CONFIG:DEBUG>:/RTCs>")
#else ()
#    add_compile_options (-O3 -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar)
#    if (USE_ASAN)
#        add_compile_options (-fsanitize=address)
#    endif ()
#endif ()
if (MSVC)
    add_compile_options ("$<$<CONFIG:DEBUG>:/RTCs>")
else ()
    add_compile_options (-g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0)
    if (USE_ASAN)
        add_compile_options (-fsanitize=address)
    endif ()
endif ()
include_directories (external/include)
link_directories (${PROJECT_SOURCE_DIR}/external/lib)

find_package(PkgConfig REQUIRED)

set(FFMPEG_PREFIX "" CACHE PATH "Prefix where FFmpeg was installed")
if (FFMPEG_PREFIX)
    set(ENV{PKG_CONFIG_PATH}   "${FFMPEG_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    set(ENV{PKG_CONFIG_LIBDIR} "${FFMPEG_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_LIBDIR}")
endif()

# IMPORTANT: do NOT use IMPORTED_TARGET here
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
    libswresample
)

message(STATUS "FFMPEG_LIBRARY_DIRS=${FFMPEG_LIBRARY_DIRS}")
message(STATUS "FFMPEG_LIBRARIES=${FFMPEG_LIBRARIES}")
message(STATUS "FFMPEG_LDFLAGS=${FFMPEG_LDFLAGS}")

if (WIN32)
    set (OPENAL_LIBRARIES openal)
    set (OPENGL_LIBRARIES glew32 opengl32)
    set (WIN32_LIBRARIES ws2_32 winmm)
elseif (APPLE)
    set (OPENAL_LIBRARIES "-framework OpenAL")
    set (OPENGL_LIBRARIES "-framework OpenGL")
else ()
    set (OPENAL_LIBRARIES openal)
    set (OPENGL_LIBRARIES GL)
    set (PTHREAD_LIBRARIES pthread)
endif ()

if (WIN32)
    set (COMPAT_SOURCES compat_mss.c)
else ()
    set (COMPAT_SOURCES compat.c compat_mss.c)
endif ()

add_executable (
    out
    GAME1.c
    GAME2.c
    GAME3.c
    GAME4.c
    GAME5.c
    GAME_data.c
    cdrom.c
    draw.c
    imm.c
    input.c
    main.cpp
    movie.c
    sm.c
    string.c
    win.c
    "${COMPAT_SOURCES}"
)

# Rename only the definition in GAME3.c: its sub_4F4E50 body becomes sub_4F4E50__abi_raw.
#set_source_files_properties(GAME3.c PROPERTIES
#    COMPILE_DEFINITIONS "sub_4F4E50=sub_4F4E50__abi_raw"
#)

# Rename only the definitions in GAME4.c so the bodies become *_abi_raw.
#set_source_files_properties(GAME4.c PROPERTIES
#    COMPILE_DEFINITIONS
#      "sub_50A5C0=sub_50A5C0__abi_raw;sub_531E20=sub_531E20__abi_raw"
#)

# Rename only the definition in GAME5.c: its sub_549380 body becomes sub_549380__abi_raw.
#set_source_files_properties(GAME5.c PROPERTIES
#    COMPILE_DEFINITIONS
#      "sub_549220=sub_549220__abi_raw;sub_549380=sub_549380__abi_raw"
#)

# ---- ABI wrapper integration ----
add_library(abi_wrappers STATIC ${PROJECT_SOURCE_DIR}/abi_wrappers.c)
set_target_properties(abi_wrappers PROPERTIES C_STANDARD 99)

# Base libs
target_link_libraries(out PRIVATE
    SDL2
    ${OPENAL_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PTHREAD_LIBRARIES}
    ${WIN32_LIBRARIES}
    abi_wrappers
)

# FFmpeg from pkg-config
target_include_directories(out PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(out PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(out PRIVATE ${FFMPEG_LIBRARIES})
target_link_options(out PRIVATE ${FFMPEG_LDFLAGS_OTHER})

if (NOT WIN32 AND NOT APPLE)
    target_link_libraries(out PRIVATE m z dl)
endif()

if (FFMPEG_PREFIX)
    target_link_options(out PRIVATE "-Wl,-rpath-link,${FFMPEG_PREFIX}/lib")
endif()

#add_compile_options(-Wfatal-errors)