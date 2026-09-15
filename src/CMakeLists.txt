# CMakeLists.txt
#if (MSVC)
#    add_compile_options ("$<$<CONFIG:DEBUG>:/RTCs>")
#else ()
#    add_compile_options (-O3 -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar)
#    if (USE_ASAN)
#        add_compile_options (-fsanitize=address)
#    endif ()
#endif ()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" NOX_ARCH)

if (NOX_ARCH MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR
        "64-bit ARM/x86 builds are not yet supported. "
        "Please build with a 32-bit ARM target/toolchain instead."
    )
endif()
if (MSVC)
    add_compile_options("$<$<CONFIG:DEBUG>:/RTCs>")
else ()
    add_compile_options(-g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0)
    add_compile_options(-fcommon -Wno-error=int-conversion -Wno-error=incompatible-pointer-types)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        add_compile_options(-m32)
    endif()

    if (USE_ASAN)
        add_compile_options(-fsanitize=address)
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

# ---- SDL2 ----
# Prefer pkg-config (works with your /opt/sdl2-win32/lib/pkgconfig/sdl2.pc)
pkg_check_modules(SDL2PC QUIET sdl2)
if (SDL2PC_FOUND)
    message(STATUS "SDL2 via pkg-config: ${SDL2PC_LIBRARIES}")
    set(SDL2_INCLUDE_DIRS ${SDL2PC_INCLUDE_DIRS})
    set(SDL2_LIBRARY_DIRS ${SDL2PC_LIBRARY_DIRS})
    set(SDL2_LIBRARIES    ${SDL2PC_LIBRARIES})
    set(SDL2_LDFLAGS      ${SDL2PC_LDFLAGS_OTHER})
else()
    # fallback (still works if you pass -L via CMAKE_EXE_LINKER_FLAGS)
    set(SDL2_INCLUDE_DIRS "")
    set(SDL2_LIBRARY_DIRS "")
    set(SDL2_LIBRARIES SDL2)
    set(SDL2_LDFLAGS "")
endif()

# ---- OpenAL / OpenGL platform libs ----
if (WIN32)
    # OpenAL import lib on Windows is almost always OpenAL32
    set (OPENAL_LIBRARIES OpenAL32)

    # OpenGL base
    set (OPENGL_LIBRARIES opengl32)

    # GLEW headers are required because draw.c includes <GL/glew.h>
    # Hard-wire the prefix used by the Dockerfile to avoid CMake find quirks in cross builds.
    if (EXISTS "/opt/glew-win32/include/GL/glew.h")
        message(STATUS "Using GLEW headers: /opt/glew-win32/include")
        include_directories("/opt/glew-win32/include")
    endif()

    # Try to locate GLEW include + lib (so <GL/glew.h> compiles and gl* symbols link)
    find_path(GLEW_INCLUDE_DIR
        NAMES GL/glew.h
        PATH_SUFFIXES include
    )
    find_library(GLEW32_LIB
        NAMES glew32 libglew32.dll.a glew32.dll.a
        PATH_SUFFIXES lib
    )

    if (GLEW_INCLUDE_DIR)
        message(STATUS "Found GLEW include: ${GLEW_INCLUDE_DIR}")
        include_directories(${GLEW_INCLUDE_DIR})
    else()
        message(STATUS "GLEW headers not found; <GL/glew.h> will fail")
    endif()

    if (GLEW32_LIB)
        message(STATUS "Found GLEW lib: ${GLEW32_LIB}")
        list(APPEND OPENGL_LIBRARIES glew32)
    else()
        message(STATUS "GLEW lib not found; building without -lglew32")
    endif()

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

set(NOX_RUNTIME_SOURCES
    GAME1.c
    GAME2.c
    GAME3.c
    GAME4.c
    netextras.c
    GAME5.c
    GAME_ABI.c
    GAME_data.c
    cdrom.c
    draw.c
    imm.c
    control_server.c
    gamepad.c
    input.c
    movie.c
    sm.c
    string.c
    win.c
    lobby.c
    upnp.c
    "${COMPAT_SOURCES}"
)

if (USE_EUD_COMPAT)
    list(APPEND NOX_RUNTIME_SOURCES eud_compat.c)
    add_compile_definitions(NOX_EUD_COMPAT)
endif()

add_executable (out ${NOX_RUNTIME_SOURCES} main.cpp)

# Standalone regression tests compile a focused translation unit directly so
# they can replace selected collaborators with test doubles. MinGW's PE
# linker does not discard all unrelated sections from those units, so provide
# the rest of the runtime as an archive for their unresolved dependencies.
if (WIN32 AND BUILD_TESTING)
    add_library(nox_test_runtime STATIC ${NOX_RUNTIME_SOURCES})
    target_link_libraries(nox_test_runtime PUBLIC
        ${SDL2_LIBRARIES}
        ${OPENAL_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${PTHREAD_LIBRARIES}
        ${WIN32_LIBRARIES}
        ${FFMPEG_LIBRARIES}
    )
    target_include_directories(nox_test_runtime PUBLIC ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(nox_test_runtime PUBLIC
        ${SDL2_LIBRARY_DIRS}
        ${FFMPEG_LIBRARY_DIRS}
    )
    target_link_options(nox_test_runtime PUBLIC ${SDL2_LDFLAGS} ${FFMPEG_LDFLAGS_OTHER})

    if (USE_EUD_COMPAT)
        add_executable(eud_compat_memory_test
            ${PROJECT_SOURCE_DIR}/tests/eud_compat_memory_test.c
        )
        target_include_directories(eud_compat_memory_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
        )
        target_compile_options(eud_compat_memory_test PRIVATE -ffunction-sections)
        target_link_options(eud_compat_memory_test PRIVATE -Wl,--gc-sections)
        target_link_libraries(eud_compat_memory_test PRIVATE nox_test_runtime)
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(eud_compat_memory_test PRIVATE -m32)
            target_link_options(eud_compat_memory_test PRIVATE -m32)
        endif()
        add_test(NAME eud_compat_memory_test COMMAND eud_compat_memory_test)
    endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    target_link_options(out PRIVATE -m32)
endif()

# ---- ABI wrapper integration ----
add_library(abi_wrappers STATIC ${PROJECT_SOURCE_DIR}/abi_wrappers.c)
set_target_properties(abi_wrappers PROPERTIES C_STANDARD 99)

if (WIN32 AND BUILD_TESTING)
    target_link_libraries(nox_test_runtime PUBLIC abi_wrappers)
endif()

# SDL2 include/link dirs (from pkg-config when available)
if (SDL2_INCLUDE_DIRS)
    target_include_directories(out PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if (SDL2_LIBRARY_DIRS)
    target_link_directories(out PRIVATE ${SDL2_LIBRARY_DIRS})
endif()

#if (WIN32 AND MINGW)
#  target_sources(out PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/win_trace_wrap.c
#  )
#
#  target_link_options(out PRIVATE
#    "-Wl,--wrap=fopen"
#
    # stdcall-decorated on i686
  #"-Wl,--wrap=__imp__RegOpenKeyExA@20"
  #"-Wl,--wrap=__imp__RegQueryValueExA@24"
  #"-Wl,--wrap=__imp__RegCloseKey@4"
#  )
#endif()

# Base libs
target_link_libraries(out PRIVATE
    ${SDL2_LIBRARIES}
    ${OPENAL_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${PTHREAD_LIBRARIES}
    ${WIN32_LIBRARIES}
    abi_wrappers
)

if (SDL2_LDFLAGS)
    target_link_options(out PRIVATE ${SDL2_LDFLAGS})
endif()

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
