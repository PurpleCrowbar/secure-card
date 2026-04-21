# Secure Card

A 1v1, direct peer-to-peer trading card game implementation. Utilises mental poker protocols and commitment schemes to prevent or detect all possible forms of cheating.

**This project is currently only officially compatible with Windows 10/11.**

## Prerequisites

[MSYS2](https://www.msys2.org) with the following `pacman` packages installed in the MinGW64 environment:
 
- GCC: `mingw-w64-x86_64-gcc`
- CMake: `mingw-w64-x86_64-cmake`
- Ninja: `mingw-w64-x86_64-ninja`
- Git: `git`

Dependencies ([SFML](https://github.com/sfml/sfml), [libsodium](https://github.com/jedisct1/libsodium)) are fetched automatically on first build.

**MSVC is not supported for this project.**

## Installation

Open the MSYS2 MinGW64 shell, navigate to the project's root, and run the following:

```bash
cmake -S . -B build -G "Ninja"
cmake --build build
```

## Running the game

Run `start_game.cmd`. This starts two processes: one with the `host` flag set, and the other with the `client` flag set.
