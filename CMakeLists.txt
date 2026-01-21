cmake_minimum_required (VERSION 3.11)
project (nox-port)

option (USE_ASAN "Enable address sanitizer" OFF)

add_subdirectory (src)

# Hide warnings in build output (Ninja / Makefiles)
set(CMAKE_RULE_MESSAGES OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# For Ninja specifically: only show failures
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)  # optional, helps long lines
