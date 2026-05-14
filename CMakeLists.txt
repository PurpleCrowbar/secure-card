cmake_minimum_required(VERSION 3.28)
project(secure_card)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    SYSTEM
)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SFML)

find_package(PkgConfig REQUIRED)
pkg_check_modules(sodium REQUIRED libsodium)

find_library(SODIUM_STATIC_LIB
    NAMES libsodium.a
    HINTS ${sodium_LIBRARY_DIRS}
    REQUIRED
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)
add_executable(secure_card ${SOURCES})

target_include_directories(secure_card PRIVATE ${sodium_INCLUDE_DIRS})
target_link_libraries(secure_card PRIVATE SFML::Graphics SFML::Window SFML::Network SFML::System ${SODIUM_STATIC_LIB})

if (WIN32)
    target_compile_definitions(secure_card PRIVATE SODIUM_STATIC)
endif()

# Only statically link runtime if building with MinGW
if (MINGW)
    target_link_options(secure_card PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

add_custom_command(
    TARGET secure_card POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:secure_card>/resources"
    COMMENT "Copying resources to build directory"
)

if (WIN32)
    file(GENERATE
        OUTPUT "$<TARGET_FILE_DIR:secure_card>/start_game.cmd"
        CONTENT "start secure_card.exe host\nstart secure_card.exe client\n"
    )
else()
    file(GENERATE
        OUTPUT "$<TARGET_FILE_DIR:secure_card>/start_game.sh"
        CONTENT "#!/bin/sh\n./secure_card host &\n./secure_card client &\nwait\n"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()
