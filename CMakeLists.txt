cmake_minimum_required(VERSION 3.20)

project(sen0658_pc
    VERSION 1.0.0
    DESCRIPTION "PC Modbus-RTU reader for DFRobot SEN0658 RS485 9-in-1 weather sensor"
    LANGUAGES CXX
)

option(SEN0658_BUILD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(sen0658 STATIC
    src/crc16.cpp
    src/modbus_rtu.cpp
    src/sen0658.cpp
)

if(WIN32)
    target_sources(sen0658 PRIVATE src/serial_port_win32.cpp)
else()
    target_sources(sen0658 PRIVATE src/serial_port_posix.cpp)
endif()

target_include_directories(sen0658 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(sen0658 PUBLIC cxx_std_17)

target_compile_options(sen0658 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if(SEN0658_BUILD_WARNINGS_AS_ERRORS)
    target_compile_options(sen0658 PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
    )
endif()

add_executable(sen0658_poll apps/sen0658_poll.cpp)
target_link_libraries(sen0658_poll PRIVATE sen0658)
target_compile_features(sen0658_poll PRIVATE cxx_std_17)

target_compile_options(sen0658_poll PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

install(TARGETS sen0658_poll RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
