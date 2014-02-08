SeismicDuck
===========

Seismid Duck is a reflection seismology game.

Installer with Pre-Built Binary
===============================

An installer Seismic Duck be dowloaded from http://www.blonzonics.us/games/seismic-duck/windows .

Building from Source
====================

You will need the following packages:
* [SDL 2 development library](http://www.libsdl.org/download-2.0.php) 
* [SDL_image 2.0 development library](http://www.libsdl.org/projects/SDL_image)
* [Intel(R) Threading Building Blocks (Intel(R) TBB)](https://www.threadingbuildingblocks.org/download)

Windows
-------
Visual Studio 2013 suffices.  The solution file is in ```SeismicDuck\Platform\SDL-2\VS2013```.  
It assumes the following locations:
* TBB is in ```$(TBB40_INSTALL_DIR)```
* SDL2 headers are in ```C:\lib\SDL2\include``` and the corresponding libraries are in ```C:\lib\SDL2\lib\x86```
* SDL_image headers are in ```C:\lib\SDL2_image\include``` and the correspoinding libraries are in ```C:\lib\SDL2_image\lib\x86```
