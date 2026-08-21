# Building a Dungeons & Tamirs Windows Executable

These steps cross-compile Dungeons-Tamirs on a Linux machine into a
Windows executable, using MinGW-w64 as the cross-compiler. Just copy/paste these commands.

Tested on Ubuntu/Debian. If you're on Fedora/Arch/..., swap the `apt`
install line for your distro's package manager (package names are similar:
`mingw64-gcc`, `mingw-w64-gcc`, etc.).

## 1. Install the required tools

```bash
sudo apt update
sudo apt install -y git cmake make mingw-w64
```

This gives you:
- `git` - to clone the repo
- `cmake` and `make` - the build system
- `mingw-w64` - the GCC cross-compiler that targets Windows
  (installs as `x86_64-w64-mingw32-gcc`)

Confirm the cross-compiler is installed:

```bash
x86_64-w64-mingw32-gcc --version
```

You should see a GCC style version banner. If instead you get "command not
found", the mingw-w64 install didn't work - re-run the install command
above and check for errors.

## 2. Get the source code

```bash
git clone https://github.com/pook27/Dungeons-Tamirs.git
cd Dungeons-Tamirs
```

## 3. Create a CMake "toolchain file"

This is a small config file that tells CMake "compile for Windows, using
the mingw compiler" instead of building for the Linux machine you're
sitting at. Save this as `mingw-toolchain.cmake` in the repo folder:

```bash
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

You only ever need to create this file once - it's not specific to this
game, you can reuse it for any CMake project you want to cross-compile.

## 4. Configure the build

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
```

What the flags mean:
- `-DCMAKE_TOOLCHAIN_FILE=...` - use the mingw toolchain file from step 3
  so CMake cross-compiles for Windows instead of Linux
- `-DCMAKE_BUILD_TYPE=Release` - build an optimized release binary
  (leave this off, or use `Debug`, if you want debug symbols instead)

This step also downloads and configures raylib (the game library the
project depends on) automatically - that's handled by the project's own
`CMakeLists.txt`, you don't need to install raylib yourself. It needs
internet access the first time; after that it's cached in `build/`.

## 5. Build it

Still inside `build/`:

```bash
cmake --build . -j$(nproc)
```

`-j$(nproc)` just parallelizes the build across all your CPU cores to
speed it up - you can drop it if you want.

This will take a few minutes the first time (it's compiling raylib from
source too). Once done, you should see:

```
[100%] Built target dungeons_and_tamirs
```

## 6. Find your output

Inside `build/` you'll now have:

```
build/dungeons_and_tamirs.exe   <- the Windows executable
build/assets/                   <- game assets, auto-copied here
```

The project's `CMakeLists.txt` automatically copies the `assets/` folder
next to the .exe after every build, so you don't need to do that by hand.

You can confirm it's a real Windows binary (even while on Linux) with:

```bash
file dungeons_and_tamirs.exe
```

It should say something like `PE32+ executable ... for MS Windows`.

## 7. Package it up to share/run on Windows

The `.exe` needs the `assets/` folder sitting right next to it - it loads
images/fonts using relative paths like `assets/tamir.png`. So copy both
together into one folder:

```bash
mkdir -p ../dist/Dungeons-Tamirs
cp dungeons_and_tamirs.exe ../dist/Dungeons-Tamirs/
cp -r assets ../dist/Dungeons-Tamirs/
cd ../dist
zip -r Dungeons-Tamirs.zip Dungeons-Tamirs
```

(If `zip` isn't installed: `sudo apt install -y zip`)

## 8. Run it on Windows

Copy `Dungeons-Tamirs.zip` to a Windows machine, unzip it
anywhere, and double-click `dungeons_and_tamirs.exe`. Keep the `assets`
folder in the same directory as the .exe - don't move the .exe out on
its own.

## Rebuilding after code changes

You don't need to redo everything from scratch. From the repo root:

```bash
cd build
cmake --build . -j$(nproc)
```

Only re-run the `cmake -DCMAKE_TOOLCHAIN_FILE=...` configure step (step 4)
if you delete the `build-win` folder or change `CMakeLists.txt` itself.

## Troubleshooting

- **"cmake: command not found"** - the `cmake` package didn't install;
  re-run step 1.
- **Configure step fails trying to reach github.com** - raylib is fetched
  from GitHub during configure; make sure you have internet access, or
  check a firewall/proxy isn't blocking `github.com`.
- **Build fails partway with a compiler error** - make sure you're
  actually using the mingw compiler, not your system gcc. Check with
  `head -20 build/CMakeCache.txt | grep COMPILER`, it should point to
  `x86_64-w64-mingw32-gcc`, not `/usr/bin/gcc`.
