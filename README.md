Seismic Duck
============

_Seismic Duck_ is a reflection seismology game.  See
http://www.blonzonics.us/games/seismic-duck for an introduction to it.

Pre-Built Version for Windows
=============================

An installer for DirectX-based _Seismic Duck_ can be dowloaded from
http://www.blonzonics.us/games/seismic-duck/windows .

Building from Source
====================

Prerequisites
-------------
For the SDL2 version, you will need the following packages:
* [Intel(R) Threading Building Blocks (Intel(R) TBB)](https://www.threadingbuildingblocks.org/download)
* [SDL 2 development library](http://www.libsdl.org/download-2.0.php)
* [SDL_image 2.0 development library](http://www.libsdl.org/projects/SDL_image)

Windows
-------

_Seismic Duck_ for Windows can be built using SDL2 or DirectX 9.0c.  In the long term,
I would like to move to SDL2 exclusively.  However, I am currently maintaining
both ports because the DirectX 9.0c version is capable of almost twice the
frame rate of the SDL2 version.

### SDL2 Version

You will need some version of Visual Studio 2013.  The Express edition suffices.
The solution file is in `Platform\SDL-2.0\VS2013` and
assumes that prerequisites are in the following locations:
* TBB is in `$(TBB40_INSTALL_DIR)`.
* SDL2 headers are in `C:\lib\SDL2\include` and the libraries
  are in `C:\lib\SDL2\lib\x86`.
* SDL_image headers are in `C:\lib\SDL2_image-2.0.0\include` and the libraries are in `C:\lib\SDL2_image-2.0.0\lib\x86`.

### DirectX 9.0c Version

You will need Visual Studio 2010, or rely on up-conversion to a newer version.
The Express edition suffices.
The solution file is in `SeismicDuck\Platform\DirectX9\VS2010` and
assumes that TBB is in `$(TBB40_INSTALL_DIR)`

MacOS
=====

_Seismic Duck_ for MacOS uses SDL2.
The Makefile assumes TBB is installed in `/usr/local/tbb/`, i.e. the include 
and lib paths are:
* `/usr/local/tbb/include`
* `/usr/local/tbb/lib`

The build process is Unix style with a Makefile.

1.  `cd Platform\SDL-2.0\MacOS`
2.  Run `make`, which should build a bunch of `.o` files and link them to an executable `seismic-duck-2.0`.
3.  Add directory path of `libtbb.dylib` to `DYLD_LIBRARY_PATH`.
4.  Run `seismic-duck-2.0`.

Linux
=====

There are no ports yet to Linux.  In principle the SDL2 version should
be straightforward to port to other platforms.  Please file an issue if you run
into problems doing the port.  If you get a port working, please consider
contributing your changes.
