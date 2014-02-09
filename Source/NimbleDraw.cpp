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
 Graphics layer for Seismic Duck
*******************************************************************************/

#include "AssertLib.h"
#include "NimbleDraw.h"
#include "Utility.h"
#include <cstring>

NimbleColor NimblePixMap::color( NimblePixel p ) const {
    NimbleColor c;
    // Note that our video card differs from description of 16-bit color on page 92 of 
    // Windows Game Programming for Dummies.  Our card has a 6-bit green channel.
    if( sizeof(NimblePixel)==2 ) {
        // 16-bit color
        c.red = p>>8&0xF8;
        c.green = p>>3&0xFC;
        c.blue = p<<3&0xF8;
    } else {
        Assert( sizeof(NimblePixel)==4 );
        c.red = p>>16 & 0xFF;
        c.green = p>>8 & 0xFF;
        c.blue = p & 0xFF;
    }
    return c;
}

void NimblePixMap::setBitPixelDepth( int bitsPerPixel ) {
    int d = 0;
    switch( bitsPerPixel ) {
        case 8: d = 3; break;
        case 16: d = 4; break;
        case 32: d = 5; break;
        default: Assert(0);
    }
    // In old versions of NimblePixMap, d was assigned to a field this->myLgBitPixelDepth.
    // But the current version assumes that pixels have a size known at compilation time,
    // hence the assignment as been replaced by an assertion.
    Assert( lgBitPixelDepth()==d );
}

void NimblePixMap::shift( int deltaX, int deltaY ) {
    myBaseAddress = (NimblePixel*)((byte*)myBaseAddress + deltaX*bytePixelDepth() + deltaY*bytesPerRow());
}

void NimblePixMap::adjustTop( int delta ) {
    Assert(delta<=myHeight);
    myBaseAddress = (NimblePixel*)((byte*)myBaseAddress + delta*bytesPerRow());
    myHeight -= delta;
}

NimblePixMap::NimblePixMap( int width, int height, int bitsPerPixel, void* base, int bytesPerRow ) :
    myBaseAddress((NimblePixel*)base),
    myBytesPerRow(bytesPerRow),
    myWidth(width),
    myHeight(height)
{
    setBitPixelDepth(bitsPerPixel);
    Assert( width>=0 );
    Assert( width==myWidth );   // Fails if width does not fit in myWidth
    Assert( height>0 );
    Assert( height==myHeight ); // Fails if height does not fit in myHeight
    Assert( base!=NULL );
} 

NimblePixMap::NimblePixMap( const NimblePixMap& src, const NimbleRect& rect ) {
    myBytesPerRow = src.myBytesPerRow;
    myHeight = rect.height();
    myWidth = rect.width();
    myBaseAddress = (NimblePixel*)((byte*)src.myBaseAddress + rect.top*myBytesPerRow + (rect.left<<lgBytePixelDepth()));
}

void NimblePixMap::draw( const NimbleRect& r, NimblePixel pixel ) {
    Assert( 0<=r.top );
    Assert( r.bottom<=height() );
    int xl = Max(0,int(r.left));
    int xr = Min(int(r.right),width() );
    int w = xr-xl;
    for( int y=r.top; y<r.bottom; ++y ) {
        NimblePixel* out = (NimblePixel*)at(xl,y);
        for( int x=0; x<w; ++x )
            *out++ = pixel;
    }
}

void NimblePixMap::drawOn( NimblePixMap& dst, int x, int y ) const {
    Assert( dst.lgBitPixelDepth()==lgBitPixelDepth() );
    // Clip left side of source
    int w = width();
    int j = 0;
    if( x<0 ) {
        w+=x;
        if( w<=0 ) return;
        j-=x;
        x=0;
    }
    // Clip right side of source
    if( x+w>dst.width() ) {
        w = dst.width()-x;
        if( w<=0 ) return;
    }
    // Clip bottom of source
    int h = height();
    if( y+h>dst.height() ) {
        h = dst.height()-y;
        if( h<=0 ) return;
    }
    for( int i=Max(0,-y); i<h; ++i ) {
        std::memcpy( dst.at(x,y+i), at(j,i), w*sizeof(NimblePixel) );
    }
}

void NimblePixMapWithOwnership::deepCopy( const NimblePixMap& src ) {
    // In all current use cases, the destination map is initially empty.
    Assert(!myBaseAddress);
    int h = myHeight = src.height();
    int w = myWidth = src.width();
    // Use operator new here to get most favorable alignment.
    myBaseAddress = (NimblePixel*)operator new(h*w*sizeof(NimblePixel));
    myHeight = h;
    myBytesPerRow = w*sizeof(NimblePixel);
    for( int y=0; y<h; ++y )
        memcpy( at(0,y), src.at(0,y), w*sizeof(NimblePixel) );
}

NimblePixMapWithOwnership::~NimblePixMapWithOwnership() {
    operator delete(myBaseAddress);
}
    
