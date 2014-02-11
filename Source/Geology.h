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

class NimblePixMap;

class RandomSource {
public:
    RandomSource() : mySize(0) {}
    void randomize() {mySize=0;}
private:
    static const size_t maxSize = 32;
    //! Length of initialized prefix in randomSequence
    mutable size_t mySize;
    mutable unsigned short myArray[maxSize];
    friend class RandomStream;
};

//! Maximum number of anticlines
const int GEOLOGY_NBUMP_MAX = 8;

//! Maximum number of faults
const int GEOLOGY_NFAULT_MAX = 8;

//! Enumeration of various layers in reservoir model.
enum GeologyLayer {
    OCEAN = 0,
    TOP_SHALE = 1,
    MIDDLE_SANDSTONE = 2,
    BOTTOM_SHALE = 3,
    GEOLOGY_N_LAYER = 4 
};

class GeologyParameters {
public:
    //! Number of bell curves to be summed
    int nBump;      
    //! Average number of faults
    int nFault; 
    //! Fractional depth (1.0 = bottom of screen)
    float oceanDepth;
    //! Fractional depth of sandstone (0=water bottom, 1=bottom of screen)
    float sandstoneDepth;
    //! Fractional curvature (ocean_depth+oceanDepth should be <= 0.9)
    float curvature;    
    //! Dip.  Should be between 0 and 1.0.
    float dip;
    //! Source of random numbers
    RandomSource random;
    GeologyParameters() : 
        nBump(1), 
        nFault(0), 
        oceanDepth(0), 
        sandstoneDepth(0.5),
        curvature(0.25f),
        dip(0)
    {}
};

class Geology {
public:
#if 0
    //! Return true iff (u,v) coordinate is in sandstone.
    bool isSandstone( int u, int v ) const {
        return myBottom[TOP_SHALE][u] <= v && v < myBottom[SANDSTONE][u];
    }
        
    bool isOcean( short local_h, short local_v ) {
        return local_v < my_bottom[OCEAN][local_h];
    }
        // Return true iff coordinate is in water.
#endif
    //! Return layer for given pixel-scale coordinates.
    /** x is relative to left side of hidden border. */
    GeologyLayer layer( int x, int y ) const {
        Assert( GEOLOGY_N_LAYER==4 );
        Assert( 0<=x && x<myWidth );
        Assert( 0<=y && y<myHeight );
        const short* column = myBottom[x];
        int k = y<column[1] ? (y<column[0] ? 0 : 1)
                            : (y<column[2] ? 2 : 3);
        return GeologyLayer(k);
    }

    //! Return reservoir v coordinate for bottom cell with given reservoir u coordinate in given layer.
    int layerBottomCell( GeologyLayer layer, int u ) const {
        Assert( OCEAN<=layer && layer<GEOLOGY_N_LAYER-1 );
        Assert( 0<=u && u<maxWidth );
        int sum = 0; 
        for( int x=0; x<RESERVOIR_SCALE; ++x ) 
            sum+=myBottom[u*RESERVOIR_SCALE+x][layer];
        // Divide by *square* scale - one factor for averaging and one for v to y conversion.
        return sum / (RESERVOIR_SCALE*RESERVOIR_SCALE);
    }

    //! Generate new geology
    void generate(const GeologyParameters& parameters, int width, int height);

    int width() const {return myWidth;}

    int height() const {return myHeight;}

    int oceanFloor() const {return myOceanFloor;}

    void drawHole( NimblePixMap& map, int x, int y ) const;

    static const int maxWidth = WAVEFIELD_VISIBLE_WIDTH_MAX+2*HIDDEN_BORDER_SIZE;
private:
    int myWidth;
    int myHeight;
    short myOceanFloor;
    //! myBottom[layer][x] is coordinate of bottom of nth layer.
    /** x is a horizontal pixel coordinate relative to left side of hidden border. */
    short myBottom[maxWidth][GEOLOGY_N_LAYER-1];
};

extern Geology TheGeology;
