cmake_minimum_required(VERSION 3.28)
project(secure_card)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
)
FetchContent_MakeAvailable(SFML)

add_executable(secure_card src/main.cpp)
target_link_libraries(secure_card PRIVATE sfml-graphics sfml-window sfml-network sfml-system)
