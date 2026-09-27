cmake_minimum_required(VERSION 3.14)

project(asterix
    VERSION 1.0.0
    DESCRIPTION "Decoder for EUROCONTROL ASTERIX categories 034, 048 and 063"
    LANGUAGES CXX)

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(ASTERIX_TOP_LEVEL ON)
else()
    set(ASTERIX_TOP_LEVEL OFF)
endif()

option(ASTERIX_BUILD_TOOLS "Build the asterix-dump command line tool" ${ASTERIX_TOP_LEVEL})
option(ASTERIX_BUILD_TESTS "Build the unit tests" ${ASTERIX_TOP_LEVEL})
option(ASTERIX_BUILD_EXAMPLES "Build the examples" ${ASTERIX_TOP_LEVEL})
option(ASTERIX_BUILD_FUZZER "Build the libFuzzer harness (requires Clang)" OFF)
option(ASTERIX_SANITIZE "Build everything with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ASTERIX_INSTALL "Generate install rules" ${ASTERIX_TOP_LEVEL})

if(ASTERIX_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(ASTERIX_SANITIZE)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

if(MSVC)
    set(ASTERIX_WARNINGS /W4 /permissive-)
else()
    set(ASTERIX_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

add_library(asterix
    src/cat034.cpp
    src/cat048.cpp
    src/cat063.cpp
    src/common.cpp
    src/decoder.cpp
    src/json.cpp
    src/summary.cpp)
add_library(asterix::asterix ALIAS asterix)
target_include_directories(asterix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(asterix PUBLIC cxx_std_17)
target_compile_options(asterix PRIVATE ${ASTERIX_WARNINGS})
set_target_properties(asterix PROPERTIES
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

if(ASTERIX_BUILD_TOOLS)
    add_executable(asterix-dump tools/asterix_dump.cpp tools/capture.cpp)
    target_include_directories(asterix-dump PRIVATE tools)
    target_link_libraries(asterix-dump PRIVATE asterix::asterix)
    target_compile_options(asterix-dump PRIVATE ${ASTERIX_WARNINGS})
    set_target_properties(asterix-dump PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(ASTERIX_BUILD_EXAMPLES)
    add_executable(asterix-example examples/decode_datagram.cpp)
    target_link_libraries(asterix-example PRIVATE asterix::asterix)
    target_compile_options(asterix-example PRIVATE ${ASTERIX_WARNINGS})
    set_target_properties(asterix-example PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(ASTERIX_BUILD_TESTS)
    enable_testing()
    add_executable(asterix-tests
        tests/main.cpp
        tests/test_cat034.cpp
        tests/test_cat048.cpp
        tests/test_cat063.cpp
        tests/test_capture.cpp
        tests/test_common.cpp
        tests/test_framing.cpp
        tests/test_json.cpp
        tests/test_robustness.cpp
        tools/capture.cpp)
    target_include_directories(asterix-tests PRIVATE tests tools)
    target_link_libraries(asterix-tests PRIVATE asterix::asterix)
    target_compile_options(asterix-tests PRIVATE ${ASTERIX_WARNINGS})
    set_target_properties(asterix-tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME asterix-tests COMMAND asterix-tests)
endif()

if(ASTERIX_BUILD_FUZZER)
    add_executable(asterix-fuzz fuzz/fuzz_decode.cpp)
    target_link_libraries(asterix-fuzz PRIVATE asterix::asterix)
    target_compile_options(asterix-fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(asterix-fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(ASTERIX_INSTALL)
    include(CMakePackageConfigHelpers)
    install(TARGETS asterix
        EXPORT asterixTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    if(ASTERIX_BUILD_TOOLS)
        install(TARGETS asterix-dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(EXPORT asterixTargets
        NAMESPACE asterix::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asterix)
    configure_package_config_file(cmake/asterixConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/asterixConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asterix)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/asterixConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asterixConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/asterixConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asterix)
endif()
