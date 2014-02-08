/* Copyright 1996-2010 Arch D. Robison 

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

/******************************************************************************
 Seismogram for Seismic Duck
*******************************************************************************/

#include "Config.h"
#include "NimbleDraw.h"
#include "ColorMatrix.h"
#include "Wavefield.h"
#include "Seismogram.h"
#include "Utility.h"
#include <cmath>
#include <cstring>

static float SeismogramData[SEISMOGRAM_HEIGHT_MAX][SEISMOGRAM_WIDTH_MAX];
static NimblePixel SeismogramPixels[SEISMOGRAM_HEIGHT_MAX][SEISMOGRAM_WIDTH_MAX];

static int SeismogramHeight;
static int SeismogramWidth;
static int SeismogramFront;
static int SeismogramValidPixelRows;

static bool SeismogramAutoGain = false;
static ColorFunc SeismogramColorFunc = ColorFunc(-1);
static SeismogramKind Kind = SK_CONTINUOUS;
static float Gain = 3.f;
static float GainMax = 16.0f;

#define SQUIGGLES_IMPLEMENTED 0
#if SQUIGGLES_IMPLEMENTED
static short SquiggleTable[128][16];
#endif

extern ColorMatrix GuideColors;
static NimblePixel SeismogramClut[SAMPLE_CLUT_SIZE];

static void ComputeSeismogramClut( const NimblePixMap& map, ColorFunc colorFunc ) {
    ColorFuncMakeClut( SeismogramClut, RockTypeMax, map, 0, 1, colorFunc );
}

void SeismogramReset( int width, int height ) {
    SeismogramHeight = height;
    SeismogramWidth = width;
    SeismogramFront = 0;
    SeismogramValidPixelRows = 0;
    for( int i=0; i<height; ++i )
        for( int j=0; j<width; ++j ) 
            SeismogramData[i][j] = 0;
 }

void SeismogramSetKind( SeismogramKind kind ) {
    Kind = kind;
    switch( kind ) {
        default:
            Assert(false);
        
        case SK_CONTINUOUS:
            break;
#if SQUIGGLES_IMPLEMENTED               
        case SK_SQUIGGLE:
            for( int i=0; i<128; ++i ) {
                char * row = (char*)(void*)SquiggleTable[i];
                for( int j=0; j<32; ++j ) {
                    row[j] = 0;
                }
                // The sums accomplish a form of anti-aliasing.
                if( i<64 ) {
                    for( int j=1; j<64-i; ++j ) {
                        row[16-j/4] = (row[16-j/4] - 0x3F/4) & 0x7F;
                    }
                } else {
                    for( int j=0; j<i-64; ++j ) {
                        row[16+j/4] = (row[16+j/4] + 0x3F/4) & 0x7F;;
                    }
                }
            }
#endif
            break;
    }
}

static void SeismogramUpdate( const NimblePixMap& map ) {
    // Slow down updates by factor slowDown
    static int slowDown = 1;
    static byte ticker;
    ticker=(ticker+1)%slowDown;
    if( ticker!=0 ) return;

    int h = map.height();
    int w = map.width();
    float* out = SeismogramData[SeismogramFront];
    if( ++SeismogramFront>=h )
        SeismogramFront = 0;
    WavefieldCopySurface( out, w );
    if( SeismogramValidPixelRows>0 ) 
        --SeismogramValidPixelRows;
}

static void SeismogramComputePixelRow( const NimblePixMap& map, int i ) {
    NimblePixel* out = SeismogramPixels[i];
    const float* in = SeismogramData[i];
    int w = SeismogramWidth;
    Assert( 0<w && w<=SEISMOGRAM_WIDTH_MAX );
    float gain = Gain;
    if( SeismogramAutoGain ) {
        // Set gain based on rms velocity.
        float rms = 0;
        for( int j=0; j<w; j++ )
            rms += in[j]*in[j];
        gain = rms>0 ? (SAMPLE_CLUT_SIZE/8)/std::sqrt(rms/w) : 1.0f;
        // Clip the gain.
        gain = Min( gain, GainMax );
    } 
    switch( Kind ) {
        default:
            Assert(false);
            
        case SK_CONTINUOUS: {
            for( int j=0; j<w; ++j ) {
                float v = in[j]*gain;
                // FIXME - use SSE to clip
                if( v>SAMPLE_CLUT_SIZE/2-1 ) v = SAMPLE_CLUT_SIZE/2-1;
                if( v<-(SAMPLE_CLUT_SIZE/2) ) v=-(SAMPLE_CLUT_SIZE/2);
                int u=int(v);
                *out++ = SeismogramClut[u+SAMPLE_CLUT_SIZE/2];
            }
            break;
        }
#if SQUIGGLES_IMPLEMENTED /* FIXME */
        case SQUIGGLE: {
            const int spacing = 16;     // Allow 32 pixels per squiggle
            for( j=spacing/2; j<nj; j+=spacing ) {
                unsigned vj = ((int)(gain*v[j]) >> 25) + 64;
                Assert( 0<=vj && vj<128 );
                short * in = SquiggleTable[vj];
                short n_short_per_squiggle = spacing;
                for( int k=0; k<n_short_per_squiggle; ++k ) {
                    *out++ = *in++;
                }
            }
            break;
        }
#endif
    }
}

static void SeismogramDraw( const NimblePixMap& map, ColorFunc colorFunc, bool autogain ) {  
    if( colorFunc!=SeismogramColorFunc ) {
        SeismogramColorFunc=colorFunc;
        ComputeSeismogramClut(map,colorFunc);
        SeismogramValidPixelRows = 0;
    }
    if( autogain!=SeismogramAutoGain ) {
        SeismogramAutoGain=autogain;
        SeismogramValidPixelRows = 0;
    }

    //! Copy the image out
    int h = map.height();
    int w = map.width();

    //! Compute pixels that have been invalidated because of new data or changes in mode.
    int i = SeismogramFront;
    for( ; SeismogramValidPixelRows<h; ++SeismogramValidPixelRows ) {
        if( --i<0 ) i=h-1;
        SeismogramComputePixelRow(map,i);
    }

    Assert( w<=SEISMOGRAM_WIDTH_MAX );
    Assert( h<=SEISMOGRAM_HEIGHT_MAX );
    int k = 0;
    for( int i=SeismogramFront; i<h; i++, k++ ) 
        std::memcpy( map.at(0,k), SeismogramPixels[i], w*sizeof(NimblePixel) );
    for( int i=0; i<SeismogramFront; i++, k++ ) 
        std::memcpy( map.at(0,k), SeismogramPixels[i], w*sizeof(NimblePixel) ); 
}

void SeismogramUpdateDraw( const NimblePixMap& map, NimbleRequest request, ColorFunc colorFunc, bool autogain ) {  
    if( request&NimbleUpdate )
        SeismogramUpdate(map);
    if( request&NimbleDraw )
        SeismogramDraw(map,colorFunc,autogain);
}
