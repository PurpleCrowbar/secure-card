# Secure Card

A 1v1, direct peer-to-peer trading card game implementation. Utilises mental poker protocols and commitment schemes to prevent or detect all possible forms of cheating.

## Prerequisites

### Windows

Install [MSYS2](https://www.msys2.org), then run the following command from the MSYS2 MinGW64 shell:

```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-libsodium \
    git
```

MSVC is not supported.

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libsodium-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxi-dev \
    libudev-dev \
    libfreetype-dev \
    libflac-dev \
    libvorbis-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev
```

### Other Linux Distributions

This software hasn't been tested on non-Debian distributions but should work with equivalent packages. Note that package names may subtly differ.

## Building

Run the following commands from the project root (MSYS2 MinGW64 shell if on Windows):

```bash
cmake -S . -B build -G "Ninja"
cmake --build build
```

## Running the game

From the `build/` directory:

- **Windows**: run `start_game.cmd`
- **Linux**: run `./start_game.sh`

The script launches two processes (one as the host and one as the client), allowing for local play. Online play coming soon.
