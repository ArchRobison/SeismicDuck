/* Copyright 1996-2014 Arch D. Robison 

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
 Geology model for Seismic Duck
*******************************************************************************/

#include <cstdlib>
#include <cmath>
#include <float.h>
#include "AssertLib.h"
#include "Config.h"
#include "Geology.h"
#include "NimbleDraw.h"
#include "ColorMatrix.h"

class RandomStream {
    const RandomSource& rs;
    size_t index;
public:
    RandomStream( const RandomSource& src ) : rs(src), index(0) {}
    float choose( float low, float high );
};

float RandomStream::choose( float low, float high ) {
    Assert(low<high);
    unsigned r;
    if( index<rs.mySize ) {
        r = rs.myArray[index];
    } else {
        r = std::rand();
        if( index<rs.maxSize ) {
            rs.myArray[index] = r;
            rs.mySize = index+1;
        }
    }
    ++index;
    return r*(1.0f/RAND_MAX)*(high-low)+low;
}

//! Class BumpModel models synclines and anticlines.
class BumpModel {
public:
    BumpModel( size_t n, int width, RandomStream& rs );
    
    //! Return relative displacement at given coordinate.
    /** Value is always in range [0,1].
        The value 1 represents the peak of an anticline. */
    float displacement( int x ) const {
        Assert( 0<=x && x < myWidth ); 
        return value[x];
    }
        
private:
    //! Width in pixels
    const int myWidth;
    float value[Geology::maxWidth];
    static float gauss( float x ) {
        return std::exp(-0.5f*x*x);
    }
};

BumpModel::BumpModel( size_t nBump, int width, RandomStream& rs ) : myWidth(width) {
    Assert( nBump<=size_t(GEOLOGY_NBUMP_MAX) );
    float center[GEOLOGY_NBUMP_MAX];        // Horizontal center of bell
    float amplitude[GEOLOGY_NBUMP_MAX];     // Amplitude of bell
    float sharpness[GEOLOGY_NBUMP_MAX];     // Width of bell
    const int bumpMargin = width/50+HIDDEN_BORDER_SIZE;        // Guaranteed margin between bump and visible boundary

    // Generate random gaussian bells
    for( size_t k=0; k<nBump; k++ ) {
        center[k] = rs.choose(float(bumpMargin), float(width-bumpMargin));
        amplitude[k] = rs.choose(50,150);
        sharpness[k] = 25/rs.choose(width,3*width);
    }

    // Compute sum of bumps.
    Assert( width <= Geology::maxWidth );
    float maxSum = -FLT_MAX;                // Keeps track of max un-normalized bump.
    float minSum = FLT_MAX;             // Keeps track of min un-normalized bump.
    for( int h=0; h<width; h++ ) {
    
        // Sum individual gaussian bells
        float sum = 0;
        for( size_t k=0; k<nBump; k++ ) {
            sum += amplitude[k] * gauss( (h-center[k])*sharpness[k] );
        }

        // Record sum as function of h 
        value[h] = sum;
        if( sum>maxSum )
            maxSum = sum;
        if( sum<minSum )
            minSum = sum;
    }
    // Normalize the values so that 1.0 is peak and 0.0 is bottom.
    float scale = 1.0f / (maxSum-minSum);
    for( int h=0; h<width; h++ ) {
        value[h] = (value[h] - minSum) * scale;
    }
}    

#if 0
//! Class FaultModel models faults
/** Not used yet, even as of version 1.7, because the results are not satisfactory. */
class FaultModel {
public:
    FaultModel( size_t n_fault, short h_width );
        // Build model with n faults.
        // The displacements will be in the range [0,1].
        
    float displacement( short h ) const;
        // Return relative displacement at given coordinate.  

    bool assert_okay() const;
private:
    const short my_n_fault;
    const short my_h_width;
    short fault_h[RESERVOIR_NFAULT_MAX+1];
        // [k] = h coordinate of where kth fault block ends.
    float fault_v[RESERVOIR_NFAULT_MAX+1];
        // [k] = displacement for kth fault block.
};

float FaultModel::displacement( short h ) const {
    Assert( (unsigned)my_n_fault < RESERVOIR_NFAULT_MAX );
    Assert( 0<=h && h<=my_h_width );
    int k=0;
    while( h>fault_h[k] ) {     // Sentinel at fault_h[n_fault]
        Assert( k<my_n_fault );
        ++k;
    }
    return fault_v[k];
}

FaultModel::FaultModel( size_t n_fault, short h_width ) : my_n_fault(n_fault), my_h_width(h_width) {
    Assert( n_fault<=RESERVOIR_NFAULT_MAX );
    // Choose horizontal locations of faults.
    for( int i=0; i<n_fault; i++ ) {
        int k;
        short h;
        // Pick h that is not in fault_h yet.  
        do {
            // Pick h away from sides and make it a multiple of RESERVOIR_SCALE
            // so that it does not divide a reservoir cell.
            h = Choose(16,h_width-16) / RESERVOIR_SCALE * RESERVOIR_SCALE; 
            k=0;
            while( k<i && fault_h[k]<h )
                ++k;
        } while( k<i && fault_h[k]==h );
        // Insert it
        memmove( &fault_h[k], &fault_h[k+1], (i-k)*sizeof(fault_h[0]) );
        fault_h[k] = h;
    }
    // Choose vertical displacements.  Force them to alternate.
    float sign = 0.001f * (Choose(0,1) ? 0.5f : -0.5f);
    for( int j=0; j<=n_fault; j++ ) {
        fault_v[j] = 0.5f + sign * Choose(500,1000);
        sign = -sign;
    }
    // Append the sentinel.
    fault_h[n_fault] = SHRT_MAX;  
}

bool FaultModel::assert_okay() const {
    Assert( my_n_fault<RESERVOIR_NFAULT_MAX );
    for( int k=0; k<my_n_fault; k++ ) {
        Assert( fault_h[k] < fault_h[k+1] );
    }
    return true;
}
#endif

void Geology::generate( const GeologyParameters& parameters, int width, int height ) {
    Assert( 0 <= parameters.curvature && parameters.curvature <= 1.0f );
    Assert( 0 <= parameters.curvature && parameters.curvature <= 1.0f );
    Assert( 0 <= parameters.sandstoneDepth && parameters.sandstoneDepth <= 1.0f );  

    myWidth = width;
    myHeight = height;
    myOceanFloor = height*parameters.oceanDepth;

    RandomStream rs(parameters.random);

    // Pick parameters for anticlinal bumps
    Assert( parameters.nBump>=0 );
    
    // Compute linear coefficients for general dip.
    // dip_slope is in units of 1/delta_h.
    // dip_0 is in units of 1 per delta_h.
    float dip_fuzz = rs.choose(0.5,1);
    float dip_slope = parameters.dip * dip_fuzz / (width-1);
    float dip_0 = 0;
    // Half of the time, reverse the slope.
    if( rs.choose(0,1)>0.5f ) {
        dip_0 = (width-1)*dip_slope;
        dip_slope = -dip_slope;    
    }
     
    // Compute normalized  bumps
    BumpModel bump( parameters.nBump, width, rs );    
#if 0
    FaultModel fault( parameters.n_fault, h_width );
#endif    
    // Combine dip, bumps, and fault
    float curve[Geology::maxWidth];
    float curveMin = FLT_MAX;   
    float curveMax = -FLT_MAX;  
    for( int h=0; h<width; h++ ) {
        float d = dip_0 + dip_slope*h;
        float b = bump.displacement(h)*parameters.curvature;
#if 0
        float f = fault.displacement(h)*0.1f; 
        float sum = (d+b+f);
#else
        float sum = d+b;
#endif
        curve[h] = sum;
        if( sum<curveMin ) curveMin = sum;
        if( sum>curveMax ) curveMax = sum;
    }
    
    // Minimum allowed thinness of a shale layer
    const float shaleMinThinness = 0.05f;
    // Thickness of sandstone
    const float sandstoneThickness = STUDY_DAMPING ? 0.0 : 0.1f;
    // Absolute depth of ocean bottom, in pixels.
    short oceanZero = height*parameters.oceanDepth;
    // Compute max allowed difference between highest and lowest point of sandstone's top surface.
    // Here 0 = surface and 1.0f = bottom of screen.
    float avail = 1.0f-parameters.oceanDepth-2*shaleMinThinness-sandstoneThickness; 
    float sandstoneSlack = avail-(curveMax-curveMin);
    float scale, additionalDepth;
    if( sandstoneSlack>=0 ) {
        // We have wiggle room
        additionalDepth = parameters.sandstoneDepth*sandstoneSlack;
        scale = 1.0f;
    } else {
        scale=avail/(curveMax-curveMin);
        additionalDepth = 0;
    }
    float topShaleOffset = height*(parameters.oceanDepth+shaleMinThinness+additionalDepth+scale*curveMax);
    float sandstoneOffset = topShaleOffset+height*sandstoneThickness;
    for( int h=0; h<width; h++ ) {
        float delta = height*scale*curve[h];
        myBottom[h][OCEAN] = oceanZero;
        myBottom[h][TOP_SHALE] = short(topShaleOffset - delta);
        myBottom[h][MIDDLE_SANDSTONE] = short(sandstoneOffset - delta);
        // Assertions check that layers are properly stacked.
        Assert( myBottom[h][OCEAN] < myBottom[h][TOP_SHALE] );
        Assert( myBottom[h][TOP_SHALE] < myBottom[h][MIDDLE_SANDSTONE] );
        Assert( myBottom[h][MIDDLE_SANDSTONE] < height );
    } 
}

extern ColorMatrix GuideColors;
static NimblePixel LayerPixel[GEOLOGY_N_LAYER][DRILL_DIAMETER];

static void InitializeGeologyClut( const NimblePixMap& map ) {
    const char materialOf[GEOLOGY_N_LAYER] = {0,2,1,2};
    const NimbleColor black(0);
    for( int k=0; k<GEOLOGY_N_LAYER; ++k ) {
        for( int j=0; j<DRILL_DIAMETER; ++j ) {
            NimbleColor c = GuideColors[materialOf[k]][GuideColors.width()/2];
            // Use cosine illumination law here to make borehold look round
            float f = 1-cosf((j-DRILL_DIAMETER/2)*(3.141592f/(DRILL_DIAMETER+1)));
            c.mix(black,f);
            LayerPixel[k][j]=c.pixel();
        }
    }
}

void Geology::drawHole( NimblePixMap& map, int x, int y ) const {
    static bool isGeologyClutIsInitialized = false;
    if(!isGeologyClutIsInitialized) {
        InitializeGeologyClut(map);
        isGeologyClutIsInitialized=true;
    }
    // FIXME do right clipping
    int y0=0;
    int h = x+HIDDEN_BORDER_SIZE;
    int left = x-DRILL_DIAMETER/2;
    int i0 = 0;
    int i1 = DRILL_DIAMETER;
    if( left<0 ) {
        i0 -= left;
        left=0;
    }
    NimblePixel* dstBegin = (NimblePixel*)map.at(left,y0);
    size_t dstStep = map.bytesPerRow()/sizeof(NimblePixel);
    const short* bottom = myBottom[h]; 
    for( int k=0; k<GEOLOGY_N_LAYER && y0<y; ++k ) {
        int y1 = k<GEOLOGY_N_LAYER-1 ? bottom[k] : map.height();
        Assert(y1<=map.height());
        if( y1>y ) 
            y1=y;
        Assert(y0<=y1);
        const NimblePixel* srcBegin = &LayerPixel[k][i0];
        const NimblePixel* srcEnd = srcBegin+(i1-i0);
        for(; y0<y1; ++y0 ) {
            NimblePixel* dst = dstBegin;
            for( const NimblePixel* s=srcBegin; s<srcEnd; ++s )
                *dst++ = *s;
            dstBegin += dstStep;
        }
    }   
}

Geology TheGeology;
