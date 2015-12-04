# This Makefile assumes SDL2 libraries are installed in /usr/local/lib/
# Define envrionment variable BUNDLE=1 to build the code as a MacOS style "bundle".

VPATH = ../../../Source ..

OBJ = Airgun.o AssertLib.o BuiltFromResource.o ColorFunc.o ColorMatrix.o \
    Game.o Geology.o NimbleDraw.o Parallel.o Reservoir.o Seismogram.o Sprite.o \
    TraceLib.o Wavefield.o Widget.o Host_sdl.o

# Basic configuration alternatives.  Choose one of the following settings of CPLUS_FLAGS.
#CPLUS_FLAGS = -O0 -g 
#CPLUS_FLAGS = -O2 
CPLUS_FLAGS = -O2 -DEXCLUSIVE_MODE -DASSERTIONS=0

INCLUDE = -I../../../Source -I/Library/Frameworks/SDL2.framework/Headers/ -I/Library/Frameworks/SDL2_image.framework/Headers

CPLUS_FLAGS += -mmacosx-version-min=10.7 
CPLUS = clang++ -stdlib=libstdc++ -MMD

ifdef BUNDLE
    APP_DIR = duck.app
    EXE_DIR = $(APP_DIR)/Contents/MacOS
else
    EXE_DIR = .
endif

EXE = $(EXE_DIR)/seismic-duck-2.1

$(EXE): $(OBJ) $(APP_DIR)
	$(CPLUS) -o $@ $(OBJ) $(LIB)

%.o: %.cpp
	$(CPLUS) $(CPLUS_FLAGS) $(INCLUDE) -std=c++11 -c $<

LIB = -ltbb \
-framework CoreAudio \
-framework AudioUnit \
-framework CoreFoundation \
-framework CoreGraphics \
-framework CoreServices \
-framework ForceFeedback \
-framework IOKit \
-framework AppKit \
-framework Carbon \
-framework ImageIO \
-framework ApplicationServices \
-liconv 

ifdef BUNDLE

    # Building Apple-style "bundle"
    CPLUS_FLAGS += -DHOST_RESOURCE_PATH=\"../Resources\"
    LIB += -Xlinker -rpath -Xlinker @loader_path/../Frameworks

    # Statically link SDL2
    LIB += /usr/local/lib/libSDL2.a /usr/local/lib/libSDL2_image.a 

    $(APP_DIR):
	    mkdir -p $(APP_DIR)/Contents/MacOS
	    mkdir -p $(APP_DIR)/Contents/Frameworks
	    cp $(TBBROOT)/lib/libtbb.dylib $(APP_DIR)/Contents/Frameworks/
	    install_name_tool -id @rpath/libtbb.dylib $(APP_DIR)/Contents/Frameworks/libtbb.dylib
	    mkdir -p $(APP_DIR)/Contents/Resources
	    cp ../../../Resource/*.png ../../../Resource/SeismicDuck.icns $(APP_DIR)/Contents/Resources
	    cp Info.plist $(APP_DIR)/Contents

    run: $(EXE)
	    $(EXE)

else

    # Build in current directory using dynamically linked libraries
    LIB += -framework SDL2 -framework SDL2_image

endif

clean:
	rm -rf *.o *.d *.dylib seismic-duck-2.1 duck.app

*.o: Makefile

-include *.d
