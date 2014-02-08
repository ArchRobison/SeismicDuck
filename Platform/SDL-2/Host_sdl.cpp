/* Copyright 2014 Arch D. Robison

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "SDL.h"
#include "SDL_image.h"
#include "Host.h"
#include "Game.h"
#include "BuiltFromResource.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static SDL_PixelFormat ScreenFormat;

static void ReportResourceError( const char* routine, const char* resourceName, const char* error ) {
    printf( "Internal error: %s failed %s: %s\n", routine, resourceName, error );
    HostExit();
}

void HostLoadResource(BuiltFromResourcePixMap& item) {
    const char* s = item.resourceName();
    std::string path("../../../Resource/"); // FIXME - needs to be different for installed game
    path += item.resourceName();
    path += ".png";
    if( SDL_Surface* raw = IMG_Load(path.c_str()) ) {
        if( SDL_Surface* image = SDL_ConvertSurface(raw, &ScreenFormat, 0) ) {
            SDL_FreeSurface(raw);
            if(SDL_LockSurface(image)==0) {
                NimblePixMap map(image->w, image->h, 8*sizeof(NimblePixel), image->pixels, image->pitch);
                item.buildFrom(map);
                SDL_UnlockSurface(image);
            } else {
                ReportResourceError("SDL_LockSurface", item.resourceName(), SDL_GetError());
            }
            SDL_FreeSurface(image);
        } else {
            ReportResourceError("SDL_ConvertSurface", item.resourceName(), SDL_GetError());
        }
    } else {
        ReportResourceError("IMG_Load", path.c_str(), IMG_GetError());
    }
}

double HostClockTime() {
    return SDL_GetTicks()*0.001;
}

static int NewFrameIntervalRate=1, OldFrameIntervalRate=-1;

void HostSetFrameIntervalRate(int limit) {
    NewFrameIntervalRate = limit;
}

static bool Quit;
static bool Resize = true;

void HostExit() {
    Quit = true;
}

// [i] has SDL_Scancode corresponding to HOST_KEY_...
static unsigned short ScanCodeFromHostKey[HOST_KEY_LAST];

// [i] has HOST_KEY_... corresponding to SDL_ScanCode
static unsigned short HostKeyFromScanCode[SDL_NUM_SCANCODES];

inline void Associate(SDL_Scancode code,int key) {
    Assert(unsigned(code)<SDL_NUM_SCANCODES);
    Assert(unsigned(key)<HOST_KEY_LAST);
    ScanCodeFromHostKey[key] = code;
    HostKeyFromScanCode[code] =key;
}

bool HostIsKeyDown(int key) {
    static int numKeys;
    static const Uint8* state = SDL_GetKeyboardState(&numKeys);
    int i = ScanCodeFromHostKey[key];
    return i<numKeys ? state[i] : false;
}

static void InitializeKeyTranslationTables() {
#if 0
    for( int i=' '; i<='@'; ++i )
        KeyTranslate[i] = i;
#endif
    Associate(SDL_SCANCODE_SPACE, ' ');
    for( int i='a'; i<='z'; ++i )
        Associate(SDL_Scancode(SDL_SCANCODE_A+(i-'a')), i);
    Associate(SDL_SCANCODE_RETURN, HOST_KEY_RETURN);
    Associate(SDL_SCANCODE_ESCAPE, HOST_KEY_ESCAPE);
    Associate(SDL_SCANCODE_LEFT, HOST_KEY_LEFT);
    Associate(SDL_SCANCODE_RIGHT, HOST_KEY_RIGHT);
    Associate(SDL_SCANCODE_UP, HOST_KEY_UP);
    Associate(SDL_SCANCODE_DOWN, HOST_KEY_DOWN);
}

static void PollEvents() {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type){
            /* Keyboard event */
            /* Pass the event data onto PrintKeyInfo() */
            case SDL_KEYDOWN:
                /*case SDL_KEYUP:*/
                GameKeyDown(HostKeyFromScanCode[event.key.keysym.scancode]);
                break;

                /* SDL_QUIT event (window close) */
            case SDL_MOUSEMOTION:
                GameMouseMove(NimblePoint(event.motion.x, event.motion.y));
                break;
            case SDL_MOUSEBUTTONDOWN:
                GameMouseButtonDown(NimblePoint(event.button.x, event.button.y), 0);  // FIXME set button correctly
                break;
            case SDL_MOUSEBUTTONUP:
                GameMouseButtonUp(NimblePoint(event.button.x, event.button.y), 0);  // FIXME set button correctly
                break;
            case SDL_QUIT:
                Quit = true;
                break;

            default:
                break;
        }
    }
}

static bool RebuildRendererAndTexture(SDL_Window* window , int w, int h, SDL_Renderer*& renderer, SDL_Texture*& texture) {
    if(texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if(NewFrameIntervalRate>0)
        flags |= SDL_RENDERER_PRESENTVSYNC;
    renderer = SDL_CreateRenderer(window, -1, flags);
    if(!renderer) {
        printf("Internal error: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if(!texture) {
        printf("Internal error: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    OldFrameIntervalRate = NewFrameIntervalRate;
    return true;
}

int main(int argc, char* argv[]){
    if(SDL_Init(SDL_INIT_VIDEO) == -1){
        printf("Internal error: SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    SDL_DisplayMode displayMode;
    if(SDL_GetCurrentDisplayMode(0, &displayMode)) {
        printf("Internal error: SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
        exit(1);
    }
    int w = displayMode.w;
    int h = displayMode.h;
#if !EXCLUSIVE_MODE
    w = 1024;
    h = 768;
#endif
    SDL_Window* window = SDL_CreateWindow(
        GameTitle(),                
        SDL_WINDOWPOS_UNDEFINED,    // initial x position
        SDL_WINDOWPOS_UNDEFINED,    // initial y position
        w,
        h,
#if EXCLUSIVE_MODE
        SDL_WINDOW_FULLSCREEN_DESKTOP
#else
        0
#endif
        );
    InitializeKeyTranslationTables();
    {
        SDL_Surface* surface = SDL_GetWindowSurface(window);
        ScreenFormat = *surface->format;
        ScreenFormat.format = SDL_PIXELFORMAT_ARGB8888;
        ScreenFormat.Amask = 0xFF000000;
        ScreenFormat.Ashift = 0;
        if(GameInitialize()) {
            SDL_Renderer* renderer = NULL;
            SDL_Texture* texture = NULL;
            while(!Quit) {
                if( NewFrameIntervalRate!=OldFrameIntervalRate ) 
                    if( !RebuildRendererAndTexture( window, w, h, renderer, texture ) )
                        break;
                void* pixels;
                int pitch;
                if(SDL_LockTexture(texture, NULL, &pixels, &pitch)==0) {
                    NimblePixMap screen(w, h, 8*sizeof(NimblePixel), pixels, pitch);
                    if(Resize) {
                        GameResizeOrMove(screen);
                        Resize = false;
                    }
                    GameUpdateDraw(screen, NimbleUpdate|NimbleDraw);
                    SDL_UnlockTexture(texture);
                    SDL_UpdateTexture(texture, NULL, pixels, pitch);
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, NULL, NULL);
                    SDL_RenderPresent(renderer);
                    for( int i=1; i<OldFrameIntervalRate; ++i ) 
                        SDL_RenderPresent(renderer);
                } else {
                    break;
                }
                PollEvents();
            }
            if( texture ) SDL_DestroyTexture(texture);
            if( renderer ) SDL_DestroyRenderer(renderer);
        } else {
            printf("GameInitialize() failed\n");
            return 1;
        }
    } 
    SDL_DestroyWindow(window);
    return 0;
}
