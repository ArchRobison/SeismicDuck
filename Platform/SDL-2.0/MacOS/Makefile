# This Makefile assumes SDL2 was installed using "brew"

VPATH = ../../../Source ..

OBJ = Airgun.o AssertLib.o BuiltFromResource.o ColorFunc.o ColorMatrix.o \
    Game.o Geology.o NimbleDraw.o Reservoir.o Seismogram.o Sprite.o \
    TraceLib.o Wavefield.o Widget.o Host_sdl.o

#CPLUS_FLAGS = -O0 -g 
#CPLUS_FLAGS = -O2 
CPLUS_FLAGS = -O2 -DEXCLUSIVE_MODE -DASSERTIONS=0

INCLUDE = -I../../../Source -I/usr/local/include -I/Library/Frameworks/SDL2.framework/Headers/ -I/Library/Frameworks/SDL2_image.framework/Headers

MIN = -mmacosx-version-min=10.8

seismic-duck-2.0: $(OBJ)
	clang++ $(MIN) -o $@ $(OBJ) $(LIB)

%.o: %.cpp
	clang++ $(MIN) $(CPLUS_FLAGS) $(INCLUDE) -std=c++11 -c $<

LIB = -L/usr/local/lib -lSDL2 -lSDL2_image -ltbb \
-framework CoreAudio \
-framework AudioUnit \
-framework CoreFoundation \
-framework CoreGraphics \
-framework ForceFeedback \
-framework IOKit \
-framework AppKit \
-framework Carbon \
-liconv 

clean:
	rm -f *.o seismic-duck-2.0

*.o: Makefile
