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
 Graphics layer for Seismic Duck
*******************************************************************************/

#pragma once
#ifndef NimbleDraw_H
#define NimbleDraw_H

#if defined(_WIN32)||defined(_WIN64)

#if _MSC_VER
/* Turn off pesky "possible loss of data" and "forcing value to bool" warnings */
#pragma warning(disable: 4244 4800) 
#endif
#else
#error unsupported target
#endif /* defined(WIN32)||defined(WIN64) */

#include <cstddef>
#include "AssertLib.h"

typedef unsigned char byte;

//! A point
class NimblePoint {
public:
	//! Coordinates of the point
    short x, y;
    
    //! Construct undefined point
    NimblePoint() {}

    //! Construct pointer with given coordinates
    NimblePoint( int x_, int y_ ) : x(x_), y(y_) {}
};

//! Vector subtraction of two points
inline NimblePoint operator-( NimblePoint a, NimblePoint b ) {
    return NimblePoint( a.x-b.x, a.y-b.y );
}

//! A rectangle or bounding box.
/** The bounds form the product of half-open intervals [left,right) x [top,bottom) */
class NimbleRect {
public:
    short left, right, top, bottom;

    //! Construct undefined rectangle.
    NimbleRect() {}

    //! Construct rectangle with given corners.
    NimbleRect( int left_, int top_, int right_, int bottom_ ) : 
        left(left_), right(right_), top(top_), bottom(bottom_)
    {
        Assert(left<=right);
        Assert(top<=bottom);
    }

    //! Width of rectange
    int width() const {return right-left;}

    //! Height of rectangle
    int height() const {return bottom-top;}

    //! Return *this translated by given delta
    NimbleRect translate( int dx, int dy ) const {
        return NimbleRect(left+dx, top+dy, right+dx, bottom+dy);
    }

    //! True if rectangle contains horizontal coordinate x.
    bool containsX( int x ) const {return unsigned(x-left) < unsigned(width());}

    //! True if rectangle contains vertical coordinate y. 
    bool containsY( int y ) const {return unsigned(y-top) < unsigned(height());}

    //! True if rectangle contains point p.
    bool contains( const NimblePoint& p ) const {return containsX(p.x) && containsY(p.y);}

    //! True if map completely contains rectangle rect.
    bool contains( const NimbleRect& rect ) const {
        return left <= rect.left && rect.right <= right &&
               top <= rect.top && rect.bottom <= bottom;
    }

    //! Set *this to intersection of *this and r.
    /** Warning: result may have backwards intervals. */
    void intersect( const NimbleRect r ) {
        if( r.left>left ) left=r.left;
        if( r.right<right ) right=r.right;
        if( r.top>top ) top=r.top;
        if( r.bottom<bottom ) bottom=r.bottom;
    }
};

//! A 32-bit pixel in ARGB format.
typedef unsigned NimblePixel;

//! A device-independent color
struct NimbleColor {
    typedef byte component_t;       // Always some integral type
    enum {
        FULL=0xFF                   // Full intensity for component
    };
    component_t red, green, blue;   // Always of type component_t
    NimbleColor() {}
    explicit NimbleColor( int gray ) : red(gray), green(gray), blue(gray) {}
    NimbleColor( int r, int g, int b ) : red(r), green(g), blue(b) {};
    void mix( const NimbleColor& other, float f );
};

inline void NimbleColor::mix( const NimbleColor& other, float f ) {
    Assert(0<=f && f<=1.0f);
    red = red*(1-f)+other.red*f;
    green = green*(1-f)+other.green*f;
    blue = blue*(1-f)+other.blue*f;
}

//! A view of memory as a rectangular region of NimblePixel.
/** The pixels within the map are those in the half-open interval [0,width()) x [0,height()).
    The map also provides mappings between colors and pixels.
	Historical note: NimbleDraw used to support a dynamic choice of 16-bit or 32-bit pixels. 
	But now it supports only 32-bit pixels. */
class NimblePixMap {
public:
    //! Construct undefined map.
    NimblePixMap() : myBaseAddress(0), myBytesPerRow(0) {}
    
    //! Construct map as view of memory.
    NimblePixMap( int width, int height, int bitsPerPixel, void* base, int bytesPerRow ); 

    //! Construct map for rectangular subregion of another map. 
    NimblePixMap( const NimblePixMap& src, const NimbleRect& rect ); 
    
    //! Base-2 log of bits per pixel
    /** In old versions of NimblePixMap, the depth was determined at run-time. 
        In the current version, a NimblePixel is presumed to be 32 bits. */
    int lgBitPixelDepth() const {
        Assert(sizeof(NimblePixel)==4);
        return 5;
    }
 
    // Base-2 log of bytes per pixel
    int lgBytePixelDepth() const {return lgBitPixelDepth()-3;}

    // Pixel depth in bits per pixel  
    int bitPixelDepth() const {return 1<<lgBitPixelDepth();}    

    //! Pixel depth in bytes per pixel
    int bytePixelDepth() const {return 1<<lgBytePixelDepth();}  
 
    //! Given color, convert to pixel.
    NimblePixel pixel( const NimbleColor& color ) const;

    //! Given pixel, convert to color
    NimbleColor color( NimblePixel pixel ) const;

    //! Extract alpha component from a pixel.
    NimbleColor::component_t alpha( NimblePixel pixel ) const;

    //! Width of map in pixels.
    int width() const {return myWidth;}

    //! Height of map in pixels
    int height() const {return myHeight;}

    //! Byte offset from a pixel to the pixel below it.
    /** Declared short because result is often used in multiplications, 
        and so compiler gets hint that 16x16 multiply will suffice. */
    short bytesPerRow() const {return myBytesPerRow;} 

    //! Draw rectangle using given pixel for its color.
    void draw( const NimbleRect& r, NimblePixel pixel );

    //! Draw this map onto dst.
    void drawOn( NimblePixMap& dst, int x, int y ) const;

    //! Move base address by ammount corresponding to given deltaX and deltaY
    void shift( int deltaX, int deltaY );

    //! Move top edge of map down by given number of bytes
    void adjustTop( int delta );

    //! Unchecked (in production mode) subscript into map.  Returns pointer to pixel at (x,y)
    void* at( int x, int y ) const;

    //! Return value of pixel at (x,y)
    NimblePixel pixelAt( int x, int y ) const {return *(NimblePixel*)at(x,y);}

    //! Return color of pixel at (x,y)
    NimbleColor colorAt( int x, int y ) const {return color(pixelAt(x,y));}

    //! Return alpha of pixel at (x,y)
    NimbleColor::component_t alphaAt( int x, int y ) const {return alpha(pixelAt(x,y));}

private:
    //! Pointer to pixel at (0,0)
    NimblePixel* myBaseAddress;

    //! Byte offset from a pixel to the pixel below it.
    short myBytesPerRow;

    //! Width of region in pixels.
    short myWidth;

    //! Height of region in pixels.
    short myHeight; 

    void setBitPixelDepth( int bitsPerPixel );

    // Deny access to assignment in order to prevent slicing errors with NimblePixMapWithOwnership
    void operator=( const NimblePixMap& src );
    friend class NimblePixMapWithOwnership;
};

inline void* NimblePixMap::at( int x, int y ) const {
    Assert( 0 <= x && x < width() );
    Assert( 0 <= y && y < height() );
    return (byte*)myBaseAddress + myBytesPerRow*y + (x<<lgBytePixelDepth());
}
 
inline NimblePixel NimblePixMap::pixel( const NimbleColor& c ) const {
	Assert( sizeof(NimblePixel)==4 );
	return c.red<<16 | c.green<<8 | c.blue;
}

inline NimbleColor::component_t NimblePixMap::alpha( NimblePixel pixel ) const {
    return pixel>>24;
}

//! NimblePixMap that owns its buffer.
class NimblePixMapWithOwnership: public NimblePixMap {
    void operator=( const NimblePixMapWithOwnership& ); 
    NimblePixMapWithOwnership( const NimblePixMapWithOwnership& ); 
public:
    NimblePixMapWithOwnership() {}
    void deepCopy( const NimblePixMap& src );
    ~NimblePixMapWithOwnership();
};

//! Bit mask values for requests.
enum NimbleRequest {
    NimbleUpdate=1,
    NimbleDraw=2
};

inline NimbleRequest operator|( NimbleRequest x, NimbleRequest y ) {
    return NimbleRequest(int(x)|int(y));
}

inline NimbleRequest operator&( NimbleRequest x, NimbleRequest y ) {
    return NimbleRequest(int(x)&int(y));
}

inline NimbleRequest operator-( NimbleRequest x, NimbleRequest y ) {
    return NimbleRequest(x&~y);
}

#endif /*NimbleDraw_H*/
