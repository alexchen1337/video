cmake_minimum_required(VERSION 3.15)
project(DiscordCompressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

find_package(ZLIB REQUIRED)

add_executable(discord_compressor src/discord_compressor.cpp)

target_link_libraries(discord_compressor PRIVATE ZLIB::ZLIB)

if(WIN32)
    target_link_libraries(discord_compressor PRIVATE ws2_32)
endif()

target_include_directories(discord_compressor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS discord_compressor DESTINATION bin)

