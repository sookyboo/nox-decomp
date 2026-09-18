cmake_minimum_required (VERSION 3.11)
project (nox-port)

option (USE_ASAN "Enable address sanitizer" OFF)
option (USE_EUD_COMPAT "Build and enable Panic EUD/MemoryHack compatibility" OFF)
option (USE_BOT_SUPPORT "Build native player bot support" OFF)
option (USE_EUD_MAP_TESTS "Build temporary external EUD map smoke tests" OFF)
option (USE_MANUAL_SPELL_CASTING "Enable manual spell casting input" ON)
set (EUD_MAP_PROJECT_DIR "${PROJECT_SOURCE_DIR}/build-deps/eud-maps-project" CACHE PATH
    "Panic EUD maps project checkout used by temporary map smoke tests")
set (EUD_GAMEFILES_DIR "${PROJECT_SOURCE_DIR}/build-deps/gamefiles/app" CACHE PATH
    "Nox gamefiles directory used by temporary map smoke tests")

if (USE_EUD_MAP_TESTS AND NOT USE_EUD_COMPAT)
    message(FATAL_ERROR "USE_EUD_MAP_TESTS requires USE_EUD_COMPAT=ON")
endif()

include(CTest)
add_subdirectory (src)

# Small ABI regression test.  It links only the wrapper under test (unused
# wrappers are removed by --gc-sections), so it can run without game assets.
if (BUILD_TESTING)
    if (WIN32)
        # Standalone tests provide their own main(); SDL2 otherwise remaps it
        # to SDL_main when its Windows headers are included.
        add_compile_definitions(SDL_MAIN_HANDLED)
    endif()
    # MinGW's PE linker does not garbage-collect the unused raw-wrapper
    # references in this standalone ABI harness. The ABI harness remains
    # covered on Linux i386 and ARMHF; Windows uses the native calling ABI.
    if (NOT WIN32)
    add_executable(abi_cross_test tests/abi_cross_test.c abi_wrappers.c)
    target_compile_options(abi_cross_test PRIVATE -ffunction-sections)
    target_link_options(abi_cross_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(abi_cross_test PRIVATE -m32)
        target_link_options(abi_cross_test PRIVATE -m32)
    endif()
    add_test(NAME abi_cross_test COMMAND abi_cross_test)
    endif()
    if (USE_BOT_SUPPORT)
        add_executable(bot_policy_test
            tests/bot_policy_test.c
            src/bot_policy.c
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_policy_test PRIVATE -m32)
            target_link_options(bot_policy_test PRIVATE -m32)
        endif()
        add_test(NAME bot_policy_test COMMAND bot_policy_test)
        add_executable(bot_engine_test
            tests/bot_engine_test.c
            src/bot_engine.c
            src/bot_trace.c
        )
        if (NOT MSVC)
            target_compile_options(bot_engine_test PRIVATE
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
            target_link_options(bot_engine_test PRIVATE -Wl,--gc-sections)
        endif()
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_engine_test PRIVATE -m32)
            target_link_options(bot_engine_test PRIVATE -m32)
        endif()
        add_test(NAME bot_engine_test COMMAND bot_engine_test)
        add_executable(bot_runtime_test
            tests/bot_runtime_test.c
            src/bot_runtime.c
            src/bot_policy.c
            src/bot_trace.c
        )
        if (NOT MSVC)
            target_compile_options(bot_runtime_test PRIVATE -fshort-wchar)
        endif()
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_runtime_test PRIVATE -m32)
            target_link_options(bot_runtime_test PRIVATE -m32)
        endif()
        add_test(NAME bot_runtime_test COMMAND bot_runtime_test)

        add_executable(bot_chat_test
            tests/bot_chat_test.c
            src/bot_chat.c
            src/bot_policy.c
        )
        if (NOT MSVC)
            target_compile_options(bot_chat_test PRIVATE -fshort-wchar)
        endif()
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_chat_test PRIVATE -m32)
            target_link_options(bot_chat_test PRIVATE -m32)
        endif()
        add_test(NAME bot_chat_test COMMAND bot_chat_test)

        add_executable(bot_console_test
            tests/bot_console_test.c
            src/bot_console.c
            src/bot_trace.c
        )
        if (NOT MSVC)
            target_compile_options(bot_console_test PRIVATE -ffunction-sections)
            target_link_options(bot_console_test PRIVATE -Wl,--gc-sections)
        endif()
        if (NOT MSVC)
            target_compile_options(bot_console_test PRIVATE -fshort-wchar)
        endif()
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_console_test PRIVATE -m32)
            target_link_options(bot_console_test PRIVATE -m32)
        endif()
        add_test(NAME bot_console_test COMMAND bot_console_test)

        add_executable(bot_team_test
            tests/bot_team_test.c
            src/bot_team.c
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_team_test PRIVATE -m32)
            target_link_options(bot_team_test PRIVATE -m32)
        endif()
        add_test(NAME bot_team_test COMMAND bot_team_test)

        add_executable(bot_warrior_test
            tests/bot_warrior_test.c
            src/bot_warrior.c
            src/bot_team.c
            src/bot_policy.c
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_warrior_test PRIVATE -m32)
            target_link_options(bot_warrior_test PRIVATE -m32)
        endif()
        add_test(NAME bot_warrior_test COMMAND bot_warrior_test)

        add_executable(bot_wizard_test
            tests/bot_wizard_test.c
            src/bot_wizard.c
            src/bot_policy.c
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_wizard_test PRIVATE -m32)
            target_link_options(bot_wizard_test PRIVATE -m32)
        endif()
        add_test(NAME bot_wizard_test COMMAND bot_wizard_test)

        add_executable(bot_conjurer_test
            tests/bot_conjurer_test.c
            src/bot_conjurer.c
            src/bot_policy.c
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
            target_compile_options(bot_conjurer_test PRIVATE -m32)
            target_link_options(bot_conjurer_test PRIVATE -m32)
        endif()
        add_test(NAME bot_conjurer_test COMMAND bot_conjurer_test)
    endif()

    add_executable(render_arch_test tests/render_arch_test.c)
    target_compile_definitions(render_arch_test PRIVATE SDL_MAIN_HANDLED)
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
    if (USE_MANUAL_SPELL_CASTING)
    add_executable(manual_spell_input_test
        tests/manual_spell_input_test.c
        src/GAME1.c
    )
    target_include_directories(manual_spell_input_test PRIVATE
        src/external/include
    )
    target_compile_definitions(manual_spell_input_test PRIVATE
        NOX_MANUAL_SPELL_CASTING
        NOX_MANUAL_SPELL_INPUT_TEST
    )
    if (MSVC)
        target_compile_options(manual_spell_input_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(manual_spell_input_test PRIVATE
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
    target_link_options(manual_spell_input_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(manual_spell_input_test PRIVATE -m32)
        target_link_options(manual_spell_input_test PRIVATE -m32)
    endif()
    add_test(NAME manual_spell_input_test COMMAND manual_spell_input_test)
    endif()
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
    # These tests cover the POSIX case-sensitive path compatibility layer.
    # Windows uses compat_mss.c instead and does not expose this test seam.
    if (NOT WIN32)
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
    add_executable(save_casepath_test
        tests/save_casepath_test.c
        src/compat.c
    )
    target_include_directories(save_casepath_test PRIVATE
        src/external/include
    )
    target_compile_definitions(save_casepath_test PRIVATE
        _FILE_OFFSET_BITS=64
    )
    if (MSVC)
        target_compile_options(save_casepath_test PRIVATE -ffunction-sections -fdata-sections)
    else()
        target_compile_options(save_casepath_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections -fdata-sections
        )
    endif()
    target_link_options(save_casepath_test PRIVATE -Wl,--gc-sections)
    target_link_libraries(save_casepath_test PRIVATE SDL2)
    if (NOT MSVC)
        target_link_libraries(save_casepath_test PRIVATE pthread m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(save_casepath_test PRIVATE -m32)
        target_link_options(save_casepath_test PRIVATE -m32)
    endif()
    add_test(NAME save_casepath_test COMMAND save_casepath_test)
    endif()
    add_executable(network_join_test
        tests/network_join_test.c
        src/GAME1.c
    )
    target_include_directories(network_join_test PRIVATE
        src/external/include
    )
    if (WIN32)
        target_compile_definitions(network_join_test PRIVATE
            NOX_NETWORK_JOIN_REGISTRY_TEST
        )
    endif()
    if (MSVC)
        target_compile_options(network_join_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(network_join_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(network_join_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(network_join_test PRIVATE -m32)
        target_link_options(network_join_test PRIVATE -m32)
    endif()
    add_test(NAME network_join_test COMMAND network_join_test)
    if (NOT WIN32)
    add_executable(public_games_test
        tests/public_games_test.c
        src/lobby.c
    )
    target_include_directories(public_games_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(public_games_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(public_games_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(public_games_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(public_games_test PRIVATE pthread m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(public_games_test PRIVATE -m32)
        target_link_options(public_games_test PRIVATE -m32)
    endif()
    add_test(NAME public_games_test COMMAND public_games_test)
    endif()
    add_executable(network_connect_test
        tests/network_connect_test.c
        src/GAME5.c
    )
    target_include_directories(network_connect_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(network_connect_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(network_connect_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(network_connect_test PRIVATE -Wl,--gc-sections)
    if (NOT MSVC)
        target_link_libraries(network_connect_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(network_connect_test PRIVATE -m32)
        target_link_options(network_connect_test PRIVATE -m32)
    endif()
    add_test(NAME network_connect_test COMMAND network_connect_test)
    add_executable(force_nature_render_test
        tests/force_nature_render_test.c
        src/GAME2.c
        src/GAME3.c
    )
    target_include_directories(force_nature_render_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(force_nature_render_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(force_nature_render_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(force_nature_render_test PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=sub_45A4E0
        -Wl,--wrap=sub_49AA90
    )
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(force_nature_render_test PRIVATE -m32)
        target_link_options(force_nature_render_test PRIVATE -m32)
    endif()
    add_test(NAME force_nature_render_test COMMAND force_nature_render_test)
    add_executable(obliterate_render_test
        tests/obliterate_render_test.c
        src/GAME2.c
        src/GAME3.c
    )
    target_include_directories(obliterate_render_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(obliterate_render_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(obliterate_render_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(obliterate_render_test PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=sub_45A4E0
        -Wl,--wrap=sub_49AA90
    )
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(obliterate_render_test PRIVATE -m32)
        target_link_options(obliterate_render_test PRIVATE -m32)
    endif()
    add_test(NAME obliterate_render_test COMMAND obliterate_render_test)
    add_executable(laser_direction_assembly_test
        tests/laser_direction_assembly_test.c
        src/GAME2.c
    )
    target_include_directories(laser_direction_assembly_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(laser_direction_assembly_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(laser_direction_assembly_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(laser_direction_assembly_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(laser_direction_assembly_test PRIVATE -m32)
        target_link_options(laser_direction_assembly_test PRIVATE -m32)
    endif()
    add_test(NAME laser_direction_assembly_test COMMAND laser_direction_assembly_test)
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
    target_link_libraries(gamepad_overlay_release_test PRIVATE SDL2)
    if (NOT MSVC)
        target_link_libraries(gamepad_overlay_release_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(gamepad_overlay_release_test PRIVATE -m32)
        target_link_options(gamepad_overlay_release_test PRIVATE -m32)
    endif()
    add_test(NAME gamepad_overlay_release_test COMMAND gamepad_overlay_release_test)
    add_executable(summon_update_test
        tests/summon_update_test.c
        src/GAME_ABI.c
        abi_wrappers.c
    )
    target_include_directories(summon_update_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(summon_update_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(summon_update_test PRIVATE
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
    target_link_options(summon_update_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(summon_update_test PRIVATE -m32)
        target_link_options(summon_update_test PRIVATE -m32)
    endif()
    add_test(NAME summon_update_test COMMAND summon_update_test)
    add_executable(summon_limit_test
        tests/summon_limit_test.c
        src/GAME4.c
    )
    target_include_directories(summon_limit_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(summon_limit_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(summon_limit_test PRIVATE
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
    target_link_options(summon_limit_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(summon_limit_test PRIVATE -m32)
        target_link_options(summon_limit_test PRIVATE -m32)
    endif()
    add_test(NAME summon_limit_test COMMAND summon_limit_test)
    add_executable(summon_behavior_test
        tests/summon_behavior_test.c
        src/GAME4.c
        src/GAME_ABI.c
        abi_wrappers.c
    )
    target_include_directories(summon_behavior_test PRIVATE
        src/external/include
    )
    if (WIN32)
        target_compile_definitions(summon_behavior_test PRIVATE
            NOX_SUMMON_BEHAVIOR_TEST
        )
    endif()
    if (MSVC)
        target_compile_options(summon_behavior_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(summon_behavior_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(summon_behavior_test PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=sub_535250 -Wl,--wrap=sub_5236F0
        -Wl,--wrap=sub_533900 -Wl,--wrap=sub_501960
    )
    if (NOT MSVC)
        target_link_libraries(summon_behavior_test PRIVATE m)
    endif()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(summon_behavior_test PRIVATE -m32)
        target_link_options(summon_behavior_test PRIVATE -m32)
    endif()
    add_test(NAME summon_behavior_test COMMAND summon_behavior_test)
    add_executable(cutscene_chunk_test
        tests/cutscene_chunk_test.c
        src/GAME_ABI.c
    )
    target_include_directories(cutscene_chunk_test PRIVATE
        src/external/include
    )
    if (MSVC)
        target_compile_options(cutscene_chunk_test PRIVATE -ffunction-sections)
    else()
        target_compile_options(cutscene_chunk_test PRIVATE
            -g -fno-strict-aliasing -fno-strict-overflow -fshort-wchar -O0
            -fcommon -Wno-error=int-conversion
            -Wno-error=incompatible-pointer-types -ffunction-sections
        )
    endif()
    target_link_options(cutscene_chunk_test PRIVATE -Wl,--gc-sections)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        target_compile_options(cutscene_chunk_test PRIVATE -m32)
        target_link_options(cutscene_chunk_test PRIVATE -m32)
    endif()
    add_test(NAME cutscene_chunk_test COMMAND cutscene_chunk_test)
    if (NOT WIN32)
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
    endif()

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

    if (WIN32)
        foreach (NOX_WINDOWS_TEST IN ITEMS
            render_arch_test
            mana_abi_bits_test
            x86_64_compat_test
            string_format_test
            inventory_hit_chain_test
            inventory_durability_test
            cheat_spell_test
            gamepad_radial_test
            gamepad_wordset_test
            gamepad_mouse_scaling_test
            network_join_test
            network_connect_test
            force_nature_render_test
            obliterate_render_test
            laser_direction_assembly_test
            gamepad_overlay_release_test
            summon_update_test
            summon_limit_test
            summon_behavior_test
            cutscene_chunk_test
            audio_compat_test
        )
            if (TARGET ${NOX_WINDOWS_TEST})
                target_link_libraries(${NOX_WINDOWS_TEST} PRIVATE nox_test_runtime)
                target_link_options(${NOX_WINDOWS_TEST} PRIVATE
                    -Wl,--allow-multiple-definition
                )
            endif()
        endforeach()
    endif()
endif()

# Hide warnings in build output (Ninja / Makefiles)
set(CMAKE_RULE_MESSAGES OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# For Ninja specifically: only show failures
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)  # optional, helps long lines
