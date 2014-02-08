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
 Color transfer functions for Seismic Duck
*******************************************************************************/

#include "ColorFunc.h"
#include "ColorMatrix.h"
#include "Wavefield.h"
#include <cmath>

using namespace std;

static const float LogScaleStretch = 16;

static float TransferFunc[SAMPLE_CLUT_SIZE+1];
ColorMatrix GuideColors("ColorLookupTable");

//! Inverse hyperbolic sine
inline float asinhf( float x ) {
    Assert(x>=0);
    return logf(fabsf(x)+sqrtf(x*x+1));
}

void ColorFuncMakeClut( NimblePixel clut[SAMPLE_CLUT_SIZE], int rockType, const NimblePixMap& map, float showGeology, float showSeismic, ColorFunc colorFunc ) {
    // Note: rockType==RockTypeMax is used for the seismogram coloring.
    Assert(rockType<=RockTypeMax);
    Assert(0<=showGeology && showGeology<=1);
    Assert(0<=showSeismic && showSeismic<=1);
    int w = GuideColors.width();
    int h = GuideColors.height();
    Assert(w%2==1);
    float scale = showSeismic*float(w/2);
    if( colorFunc==CF_Log ) 
        scale /= asinhf(LogScaleStretch);
    for( int j=0; j<=SAMPLE_CLUT_SIZE/2; ++j ) {
        // u is normalized amplitude in range [-1,1]
        float u = j*(2.0f/SAMPLE_CLUT_SIZE);
        float v;
        switch( colorFunc ) {
            default:
                Assert(false);
            case CF_SignOnly:
                v = u==0 ? 0 : 2.0f/3;
                break;
            case CF_Log:
                v = asinhf(u*LogScaleStretch);
                break;
            case CF_Linear:
                v = u;
                break;
        }
        TransferFunc[SAMPLE_CLUT_SIZE/2+j] = v*scale;
        TransferFunc[SAMPLE_CLUT_SIZE/2-j] = -v*scale;
    }

    // Get rock-independent color.  The "-2" is there instead of "-1" because "-1"
    // is for an unused "light" color scale as opposed to the "dark" scale at "-2".
    const NimbleColor* colorScale0 = GuideColors[h-2]+w/2;
    // Get rock-dependent color
    const NimbleColor* colorScale1 = GuideColors[rockType]+w/2;
    for( int j=0; j<SAMPLE_CLUT_SIZE; ++j ) {
        int k = int(TransferFunc[j]);
        float residue = TransferFunc[j]-k;
        Assert( k<GuideColors.width() );
        NimbleColor c0 = colorScale0[k];
        NimbleColor c1 = colorScale1[k];
        if( residue>0 ) {
            c0.mix(colorScale0[k+1],residue);
            c1.mix(colorScale1[k+1],residue);
        }
        if( residue<0 ) {
            c0.mix(colorScale0[k-1],-residue);
            c1.mix(colorScale1[k-1],-residue);
        }
        c0.mix(c1,showGeology);
        clut[j] = map.pixel(c0); 
    }
}
