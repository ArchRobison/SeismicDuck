# Prerequisites

## Intel Threading Building Blocks (Intel TBB)

Intel Threading Building Blocks can be downloaded from
[https://www.threadingbuildingblocks.org/download](https://www.threadingbuildingblocks.org/download).
You just need the "OS X" version with prebuilt libraries, not the full source.

The Makefile assumes TBB is installed in `/usr/local/tbb/`, i.e. the include 
and lib paths are:
* `/usr/local/tbb/include`
* `/usr/local/tbb/lib`
You'll need to set `DYLD_LIBRARY_PATH=/usr/local/tbb/lib/`.

## SDL-2

SDL-2 can be downloaded from [https://www.libsdl.org/download-2.0.php].
The Makefile presumes that the SDL-2 headers are in `/usr/local/include/SDL2`.

# Building and Running Seismic Duck

1. Run `make`, which should build a bunch of `.o` files and link them to an executable `seismic-duck-2.0`.
2. Add directory path of `libtbb.dylib` to `DYLIB_LIBRARY_PATH`.
3. Run `seismic-duck-2.0`.
