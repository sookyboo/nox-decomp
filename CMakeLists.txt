cmake_minimum_required (VERSION 3.11)
project (nox-port)

option (USE_ASAN "Enable address sanitizer" OFF)

add_subdirectory (src)

# Small ABI regression test.  It links only the wrapper under test (unused
# wrappers are removed by --gc-sections), so it can run without game assets.
include(CTest)
if (BUILD_TESTING)
    add_executable(abi_cross_test tests/abi_cross_test.c abi_wrappers.c)
    target_compile_options(abi_cross_test PRIVATE -ffunction-sections)
    target_link_options(abi_cross_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(abi_cross_test PRIVATE -m32)
        target_link_options(abi_cross_test PRIVATE -m32)
    endif()
    add_test(NAME abi_cross_test COMMAND abi_cross_test)
    add_executable(render_arch_test tests/render_arch_test.c)
    target_link_libraries(render_arch_test PRIVATE SDL2)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(render_arch_test PRIVATE -m32)
        target_link_options(render_arch_test PRIVATE -m32)
    endif()
    add_test(NAME render_arch_test COMMAND render_arch_test)
    add_executable(mana_abi_bits_test tests/mana_abi_bits_test.c)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(mana_abi_bits_test PRIVATE -m32)
        target_link_options(mana_abi_bits_test PRIVATE -m32)
    endif()
    add_test(NAME mana_abi_bits_test COMMAND mana_abi_bits_test)
    add_executable(x86_64_compat_test tests/x86_64_compat_test.c)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        target_compile_options(x86_64_compat_test PRIVATE -m32)
        target_link_options(x86_64_compat_test PRIVATE -m32)
    endif()
    add_test(NAME x86_64_compat_test COMMAND x86_64_compat_test)
endif()

# Hide warnings in build output (Ninja / Makefiles)
set(CMAKE_RULE_MESSAGES OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# For Ninja specifically: only show failures
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)  # optional, helps long lines
