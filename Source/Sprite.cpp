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
 Sprite support for Seismic Duck
*******************************************************************************/

#include "NimbleDraw.h"
#include "Sprite.h"
#include <cstring>

//-----------------------------------------------------------------
// Sprite
//-----------------------------------------------------------------

void SpriteRep::buildFrom( const NimblePixMap& map, int top, int bottom, bool flip ) {
    int w = map.width();
    size_t dataSize = 0;
    header* dataPointer = NULL;
    NimblePixel* tmp = flip ? new NimblePixel[w] : NULL;
    for( int pass=0; pass<2; ++pass ) {
        for( int y=top; y<bottom; ++y ) {
            const NimblePixel* left = (NimblePixel*)map.at(0,y);
            if( flip ) {
                for( int j=0; j<w; ++j )
                    tmp[w-1-j] = *left++;
                left = tmp;
            }
            const NimblePixel* right = left+w;
            const NimblePixel* p=left;
            do {
                Assert( p<right );
                // Find non-transparent pixel
                while( map.alpha(*p)<NimbleColor::FULL/2 )
                    if( ++p>=right ) 
                        goto nextRow;
                const NimblePixel* begin = p;
                // Find transparent pixel or end of row
                while( ++p<right && map.alpha(*p)>=NimbleColor::FULL/2 )
                    continue;
                Assert( begin<p );
                size_t dataBytes = (char*)p-(char*)begin; 
                if( pass==0 ) {
                    dataSize += (sizeof(header)-sizeof(NimblePixel))+dataBytes;
                } else {
                    header* h = (header*)dataPointer;
                    h->length = dataBytes/sizeof(NimblePixel);
                    h->x = begin-left;
                    h->y = y-top;
                    std::memcpy( h->pixels, begin, dataBytes );
                    dataPointer = (header*)&h->pixels[h->length];
                }
            } while( p<right );
nextRow:;
        }
        if( pass==0 ) {
            dataSize+=sizeof(header);    
            myData = dataPointer = (header*)operator new(dataSize);
        } 
    }
    // Add terminator
    dataPointer->length = 0;
    Assert( (char*)&dataPointer->pixels[1]==(char*)myData+dataSize );
    delete[] tmp;
}

void SpriteRep::drawOn( const NimblePixMap& map, int x, int y ) const {
    // Following assertion fails if SpriteRep::buildFrom was not called.
    Assert(myData!=0);
    int w = map.width();
    int h = map.height();
    for( header* ptr = myData; int n = ptr->length; ptr = (header*)&ptr->pixels[ptr->length] ) {
        int rowY = y+ptr->y;
        // Clip against top
        if( rowY<0 ) continue;
        // Clip against bottom
        if( rowY>=h ) break;

        int rowX = x+ptr->x;
        // Clip against right side
        if( rowX+n>w ) 
            n = w-rowX;
        // Clip against left side
        int m = 0;
        if( rowX<0 ) {
            m-=rowX;
            rowX=0;
        }
        if( m<n ) {
            Assert( 0<=rowX && rowX<w );
            std::memcpy( map.at(rowX,rowY), &ptr->pixels[m], sizeof(NimblePixel)*(n-m) );
        }
    }
}

//-----------------------------------------------------------------
// Sprite
//-----------------------------------------------------------------

void Sprite::buildFrom( const NimblePixMap& map ) {
    int w = map.width();
    int h = map.height();
    myWidth = w;
    myHeight = h;
    Assert( 0<w && w<1024 );
    Assert( 0<h && h<1024 );
    myRep.buildFrom(map,0,h,myFlip);
}

//-----------------------------------------------------------------
// AnimatedSprite
//-----------------------------------------------------------------

void AnimatedSprite::buildFrom( const NimblePixMap& map ) {
    myWidth = map.width();;
    const int h = myFrameHeight;
    // Failure of following assertion probably indicates that resource and height disagree.
    Assert( map.height()%h==0 );
    myNumFrames = map.height()/h;
    myRep = new SpriteRep[myNumFrames];
    for( int i=0; i<myNumFrames; ++i ) 
        myRep[i].buildFrom( map, i*h, (i+1)*h );
    myCurrentFrame = 0;
}

