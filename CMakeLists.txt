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
    add_executable(string_format_test tests/string_format_test.c src/string.c)
    target_compile_options(string_format_test PRIVATE -ffunction-sections)
    target_link_options(string_format_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(string_format_test PRIVATE -m32)
        target_link_options(string_format_test PRIVATE -m32)
    endif()
    add_test(NAME string_format_test COMMAND string_format_test)
    add_executable(inventory_hit_chain_test
        tests/inventory_hit_chain_test.c
    )
    target_include_directories(inventory_hit_chain_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(inventory_hit_chain_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(inventory_hit_chain_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(inventory_hit_chain_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(inventory_hit_chain_test PRIVATE -m32)
        target_link_options(inventory_hit_chain_test PRIVATE -m32)
    endif()
    add_test(NAME inventory_hit_chain_test COMMAND inventory_hit_chain_test)
    add_executable(inventory_durability_test
        tests/inventory_durability_test.c
        src/string.c
    )
    target_include_directories(inventory_durability_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(inventory_durability_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(inventory_durability_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(inventory_durability_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(inventory_durability_test PRIVATE -m32)
        target_link_options(inventory_durability_test PRIVATE -m32)
    endif()
    add_test(NAME inventory_durability_test COMMAND inventory_durability_test)
    add_executable(cheat_spell_test
        tests/cheat_spell_test.c
        src/GAME1.c
        src/GAME3.c
    )
    target_include_directories(cheat_spell_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(cheat_spell_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(cheat_spell_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_compile_definitions(cheat_spell_test PRIVATE
        NOX_CHEAT_REGRESSION_TEST
    )
    target_link_options(cheat_spell_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(cheat_spell_test PRIVATE -m32)
        target_link_options(cheat_spell_test PRIVATE -m32)
    endif()
    add_test(NAME cheat_spell_test COMMAND cheat_spell_test)
    add_executable(gamepad_radial_test
        tests/gamepad_radial_test.c
        src/input.c
    )
    target_include_directories(gamepad_radial_test PRIVATE
        src/external/include
    )
    target_compile_definitions(gamepad_radial_test PRIVATE
        NOX_GAMEPAD_RADIAL_TEST
    )
    if (MSVC)
        target_compile_options(gamepad_radial_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(gamepad_radial_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(gamepad_radial_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(gamepad_radial_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(gamepad_radial_test PRIVATE -m32)
        target_link_options(gamepad_radial_test PRIVATE -m32)
    endif()
    add_test(NAME gamepad_radial_test COMMAND gamepad_radial_test)
    add_executable(gamepad_wordset_test
        tests/gamepad_wordset_test.c
    )
    target_include_directories(gamepad_wordset_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(gamepad_wordset_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(gamepad_wordset_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(gamepad_wordset_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(gamepad_wordset_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(gamepad_wordset_test PRIVATE -m32)
        target_link_options(gamepad_wordset_test PRIVATE -m32)
    endif()
    add_test(NAME gamepad_wordset_test COMMAND gamepad_wordset_test)
    add_executable(gamepad_mouse_scaling_test
        tests/gamepad_mouse_scaling_test.c
    )
    target_include_directories(gamepad_mouse_scaling_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(gamepad_mouse_scaling_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(gamepad_mouse_scaling_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(gamepad_mouse_scaling_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(gamepad_mouse_scaling_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(gamepad_mouse_scaling_test PRIVATE -m32)
        target_link_options(gamepad_mouse_scaling_test PRIVATE -m32)
    endif()
    add_test(NAME gamepad_mouse_scaling_test COMMAND gamepad_mouse_scaling_test)
    add_executable(compat_path_test
        tests/compat_path_test.c
        src/compat.c
    )
    target_include_directories(compat_path_test PRIVATE
        src/external/include
    )
    target_compile_definitions(compat_path_test PRIVATE
        _FILE_OFFSET_BITS=64
    )
    if (MSVC)
        target_compile_options(compat_path_test PRIVATE -ffunction-sections -fdata-sections)
    else()
        target_compile_options(compat_path_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
            -fdata-sections
        )
    endif()
    target_link_options(compat_path_test PRIVATE -Wl,--gc-sections)
    target_link_libraries(compat_path_test PRIVATE SDL2)
    if (NOT MSVC)
        target_link_libraries(compat_path_test PRIVATE pthread m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(compat_path_test PRIVATE -m32)
        target_link_options(compat_path_test PRIVATE -m32)
    endif()
    add_test(NAME compat_path_test COMMAND compat_path_test)
    add_executable(gamepad_overlay_release_test
        tests/gamepad_overlay_release_test.c
    )
    target_include_directories(gamepad_overlay_release_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(gamepad_overlay_release_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(gamepad_overlay_release_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(gamepad_overlay_release_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(gamepad_overlay_release_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(gamepad_overlay_release_test PRIVATE -m32)
        target_link_options(gamepad_overlay_release_test PRIVATE -m32)
    endif()
    add_test(NAME gamepad_overlay_release_test COMMAND gamepad_overlay_release_test)
    add_executable(map_download_dispatch_test
        tests/map_download_dispatch_test.c
        src/GAME1.c
        src/GAME2.c
        src/GAME3.c
        src/GAME4.c
        src/netextras.c
        src/GAME5.c
        src/GAME_ABI.c
        src/GAME_data.c
        src/cdrom.c
        src/draw.c
        src/imm.c
        src/control_server.c
        src/gamepad.c
        src/input.c
        src/movie.c
        src/sm.c
        src/string.c
        src/win.c
        src/lobby.c
        src/upnp.c
        src/compat.c
        src/compat_mss.c
        abi_wrappers.c
    )
    target_include_directories(map_download_dispatch_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(map_download_dispatch_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(map_download_dispatch_test PRIVATE
            -g
            -fno-strict-aliasing
            -fno-strict-overflow
            -fshort-wchar
            -O0
            -fcommon
            -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types
            -ffunction-sections
        )
    endif()
    target_link_options(map_download_dispatch_test PRIVATE -Wl,--gc-sections)
    target_link_libraries(map_download_dispatch_test PRIVATE
        SDL2
        openal
        GL
        pthread
        avformat
        avcodec
        swscale
        swresample
        avutil
        m
        z
        dl
    )
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(map_download_dispatch_test PRIVATE -m32)
        target_link_options(map_download_dispatch_test PRIVATE -m32)
    endif()
    add_test(NAME map_download_dispatch_test COMMAND map_download_dispatch_test)

    add_executable(audio_compat_test
        tests/audio_compat_test.c
        src/compat_mss.c
    )
    target_compile_definitions(audio_compat_test PRIVATE
        NOX_AUDIO_REGRESSION_TEST
    )
    target_include_directories(audio_compat_test PRIVATE
        src/external/include
    )
    target_compile_options(audio_compat_test PRIVATE
        -ffunction-sections
        -fdata-sections
    )
    target_link_options(audio_compat_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(audio_compat_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(audio_compat_test PRIVATE -m32)
        target_link_options(audio_compat_test PRIVATE -m32)
    endif()
    add_test(NAME audio_compat_test COMMAND audio_compat_test)
endif()

# Hide warnings in build output (Ninja / Makefiles)
set(CMAKE_RULE_MESSAGES OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# For Ninja specifically: only show failures
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)  # optional, helps long lines
