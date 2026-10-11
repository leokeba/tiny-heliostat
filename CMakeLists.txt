# The heliostat's code: an ESP-IDF component when built by idf.py (the
# firmware in firmware/ adds this directory), host library + tests otherwise.
set(HELIOSTAT_SRCS
    src/heliostat/sun.cpp
    src/heliostat/geometry.cpp
    src/heliostat/tracker.cpp
    src/heliostat/heliostat.cpp
    src/heliostat/heliostat_tree.cpp)

if(ESP_PLATFORM)
    # TesserAPI and TesserMotion, under whatever component name they were
    # added with (a directory in EXTRA_COMPONENT_DIRS, or the component
    # manager's "<namespace>__<name>"), as TesserKIT finds its own.
    idf_build_get_property(HELIOSTAT_BUILD_COMPONENTS BUILD_COMPONENTS)
    set(HELIOSTAT_TESSERAPI "")
    set(HELIOSTAT_TESSERMOTION "")
    foreach(c ${HELIOSTAT_BUILD_COMPONENTS})
        string(TOLOWER "${c}" c_lower)
        if(c_lower MATCHES "(^|__)tesserapi$")
            set(HELIOSTAT_TESSERAPI ${c})
        elseif(c_lower MATCHES "(^|__)tessermotion$")
            set(HELIOSTAT_TESSERMOTION ${c})
        endif()
    endforeach()
    if((NOT HELIOSTAT_TESSERAPI OR NOT HELIOSTAT_TESSERMOTION) AND NOT CMAKE_BUILD_EARLY_EXPANSION)
        message(FATAL_ERROR "tiny-heliostat needs TesserAPI and TesserMotion as components")
    endif()
    idf_component_register(SRCS ${HELIOSTAT_SRCS} INCLUDE_DIRS "src")
    if(HELIOSTAT_TESSERAPI AND HELIOSTAT_TESSERMOTION)
        foreach(dep ${HELIOSTAT_TESSERAPI} ${HELIOSTAT_TESSERMOTION})
            idf_component_get_property(lib ${dep} COMPONENT_LIB)
            target_link_libraries(${COMPONENT_LIB} PUBLIC ${lib})
        endforeach()
    endif()
    return()
endif()

cmake_minimum_required(VERSION 3.16)
project(tiny_heliostat CXX)

option(HELIOSTAT_SANITIZE "Build with ASan/UBSan" ON)
set(TESSERMOTION_DIR "" CACHE PATH "TesserMotion source tree (default: ../TesserMotion)")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# TesserMotion, which brings TesserAPI (../TesserAPI next to it, or
# TESSERAPI_DIR), and its testing target: the fakes and the simulated plant.
if(NOT TESSERMOTION_DIR)
    get_filename_component(TESSERMOTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../TesserMotion" ABSOLUTE)
endif()
if(NOT EXISTS "${TESSERMOTION_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "TesserMotion not found at ${TESSERMOTION_DIR}: clone it there, or set TESSERMOTION_DIR")
endif()
set(TESSERMOTION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${TESSERMOTION_DIR}" "${CMAKE_BINARY_DIR}/tessermotion")

set(HELIOSTAT_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror -fno-exceptions -fno-rtti)

add_library(heliostat_core ${HELIOSTAT_SRCS})
target_include_directories(heliostat_core PUBLIC src)
target_link_libraries(heliostat_core PUBLIC tessermotion_core)
set_target_properties(heliostat_core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(heliostat_core PRIVATE ${HELIOSTAT_WARNINGS})

enable_testing()
add_executable(heliostat_tests test/test_main.cpp)
target_link_libraries(heliostat_tests PRIVATE heliostat_core tessermotion_testing)
set_target_properties(heliostat_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(heliostat_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
add_test(NAME heliostat_tests COMMAND heliostat_tests)

if(HELIOSTAT_SANITIZE)
    # Every library in the binary, so it is instrumented alike.
    foreach(t tesser_core tessermotion_core tessermotion_testing heliostat_core heliostat_tests)
        target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()
