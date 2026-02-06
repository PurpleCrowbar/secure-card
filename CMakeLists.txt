cmake_minimum_required(VERSION 3.28)
project(secure_card)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
)
FetchContent_MakeAvailable(SFML)
FetchContent_Declare(
    sodium
    URL https://download.libsodium.org/libsodium/releases/libsodium-1.0.21-stable-mingw.tar.gz
)
FetchContent_MakeAvailable(sodium)

add_executable(secure_card src/main.cpp)

target_include_directories(secure_card PRIVATE
        ${sodium_SOURCE_DIR}/libsodium-win64/include
)

add_library(sodium_lib STATIC IMPORTED)
set_target_properties(sodium_lib PROPERTIES
        IMPORTED_LOCATION ${sodium_SOURCE_DIR}/libsodium-win64/lib/libsodium.a
)

target_link_libraries(secure_card PRIVATE sfml-graphics sfml-window sfml-network sfml-system sodium_lib)
