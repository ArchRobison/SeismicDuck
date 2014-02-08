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
 Wave physics and rendering for Seismic Duck
*******************************************************************************/

#include "Config.h"
#include "NimbleDraw.h"
#include "ColorMatrix.h"
#include "ColorFunc.h"
#include "Geology.h"
#include "Wavefield.h"
#include "Airgun.h"
#include "Utility.h"
#include "SSE.h"
#include "Parallel.h"
#include <cmath>
#include <cfloat>
#include <cstring>

#if __GNUC__
#define CACHE_ALIGN(x) x __attribute__ ((aligned (16)))
#else
#define CACHE_ALIGN(x) __declspec(align(64)) x
#endif

//! Set to 1 do show tile boundaries (for debugging only).
#define DRAW_TILES 0

//! Set to 1 to show the color scale (used only when writing documentation)
#define DRAW_COLOR_SCALE 0

//! If non-zero, enables optimization calculations for tiles with homogeneous rock properties.
#define OPTIMIZE_HOMOGENEOUS_TILES 1

static const int NUM_PANEL_MAX = 16;

//! Set to 1 if experimenting with tiles bigger than 7x508
#define ALLOW_BIG_TILES 0

//! Number of panels
static int NumPanel = 10;
static int PumpFactor = 3;

static int TileHeight = 7;    // Must be <= 15.
static int TileWidth = 16*7;  // Must be multiple of 4 and <= 4*63 

//! Size of damping region, in pixels.
const int DampSize = 16;

//! Velocity of various materials.
/** The product of LFunc[k]*MFunc[k] must not exceed 0.5, otherwise runaway positive feedback occurs. 
    The values here are correctly proportioned for water and shale.
    The sandstone values are fictionally the geomean of the values for water and shale. */
//                                           WATER SANDSTONE  SHALE
static const float MofRock[RockTypeMax+1] = {0.50f,  0.3536f, 0.25f};
static const float LofRock[RockTypeMax+1] = {0.25f,  0.7071f, 2.00f};

static int WavefieldWidth;

//! Equal to viewable height plus 1 for free surface plus DampSize for bottom PML region
/** Does not inlude overlap zones. */
static int WavefieldHeight;

//! Maximum allowed width of wavefield, including left and right PML regions.
static const int WavefieldWidthMax = HIDDEN_BORDER_SIZE+WAVEFIELD_VISIBLE_WIDTH_MAX+HIDDEN_BORDER_SIZE;

//! Maximum allowed hight of wavefield, including PML region on bottom.  
/** The 1 is for the top read-only row of grid points. 
    The 4*NUM_REGION_MAX is for the overlap zones between adjacent regions. */
static const int WavefieldHeightMax = 1+WAVEFIELD_VISIBLE_HEIGHT_MAX+HIDDEN_BORDER_SIZE+4*NUM_PANEL_MAX;

//! Array of rock types, with two bits per element.
static CACHE_ALIGN( byte RockMap[WavefieldHeightMax][WavefieldWidthMax>>2] );

typedef CACHE_ALIGN( float FieldType[WavefieldHeightMax][WavefieldWidthMax] );

static FieldType Vx, Vy, U, A, B;

//! "Psi" fields for left and right PML regions.  
static CACHE_ALIGN( float Pl[WavefieldHeightMax][DampSize] );
static CACHE_ALIGN( float Pr[WavefieldHeightMax][DampSize] );

//! "Psi" field for bottom PML region.  
/** Overlaps left and right PML regions on corners. */
static CACHE_ALIGN( float Pb[DampSize][WavefieldWidthMax] );

typedef CACHE_ALIGN( float SigmaType[DampSize] );

//! Coefficients for Uniaxial Perfectly Matched Layer (UPML)
static SigmaType D0, D1, D2, D3, D4, D5;
static const float D6=1.0f-FLT_EPSILON;

static short PanelIOfYPlus1[WavefieldHeightMax];
static int PanelFirstY[NUM_PANEL_MAX+1];
static int PanelFirstI[NUM_PANEL_MAX];
static int PanelLastI[NUM_PANEL_MAX];

struct PanelTransferDesc {
    short srcI;
    short dstI;
};

static PanelTransferDesc PanelTransfer[NUM_PANEL_MAX][2*PUMP_FACTOR_MAX];

//! Effective length of each row of PanelTransfer
static int PanelTransferCount;

static inline int TrapezoidFirstI( int p, int k ) {
    Assert( 0<=p && p<NumPanel );
    Assert( 0<=k && k<PumpFactor );
    return p==0 ? PanelFirstI[p] : PanelFirstI[p]-(PumpFactor-k);
}

static inline int TrapezoidLastI( int p, int k ) {
    Assert( 0<=p && p<NumPanel );
    Assert( 0<=k && k<PumpFactor );
    return p==NumPanel-1 ? PanelLastI[p] : PanelLastI[p]+(PumpFactor-1-k);
}

inline int IofY( int y ) {
    Assert( -1<=y && y<=WavefieldHeight );
    return PanelIOfYPlus1[y+1];
}

static int TopIofBottomRegion;
static int LeftJofRightRegion;

static void InitializePanelMap() {
    Assert(1<=PumpFactor && PumpFactor<=PUMP_FACTOR_MAX);
#if ASSERTIONS
    int h = WavefieldHeight;
#endif /*ASSERTIONS*/
    int w = WavefieldWidth;
    PanelFirstY[0] = -1;
    for( int p=1; p<NumPanel; ++p )
        PanelFirstY[p] = WavefieldHeight*p/NumPanel;
    PanelFirstY[NumPanel] = WavefieldHeight-1;
    Assert( 0<h && h<=WavefieldHeightMax ); // FIXME - account for panel boundaries
    Assert( 0<w && w<=WavefieldWidthMax );
    Assert( NumPanel<=NUM_PANEL_MAX );
    int i=0;
    for( int p=0; p<NumPanel; ++p ) {
        PanelFirstI[p] = i;
        for( int y=PanelFirstY[p]; y<PanelFirstY[p+1]; ++y )
            PanelIOfYPlus1[y+1] = i++;
        PanelLastI[p] = i;
        // Allocate separation zone
        i+=2*PUMP_FACTOR_MAX+1;
    }
    TopIofBottomRegion = PanelLastI[NumPanel-1]-DampSize;
    LeftJofRightRegion = w-DampSize; 
}

static void InitializeZoneTranfers() {
    // Compute panel boundary transfers
    PanelTransferCount = 2*PumpFactor;
    for( int p=1; p<NumPanel; ++p ) {
        int k = 0;
        for( int d=0; d<PumpFactor; ++d ) {
            // Copy from start of panel p+1 to end of panel p
            PanelTransfer[p][k].srcI = PanelFirstI[p]+d;
            PanelTransfer[p][k].dstI = PanelLastI[p-1]+d;
            ++k;
        }
        for( int d=0; d<PumpFactor; ++d ) {
            // Copy from end of panel p to start of panel p+1
            PanelTransfer[p][k].srcI = PanelLastI[p-1]-d-1;
            PanelTransfer[p][k].dstI = PanelFirstI[p]-d-1;
            ++k;
        }
        Assert( k==PanelTransferCount );
    }
}

//! Initialize RockMap and related wavefield propagation coefficients.
static void InitializeRockMap( const Geology& g ) {
    int h = WavefieldHeight;
    int w = WavefieldWidth;
    Assert( 4<=WavefieldHeight && WavefieldHeight<=WavefieldHeightMax );
    Assert( 4<=WavefieldWidth && WavefieldWidth<=WavefieldWidthMax ); 

    static const RockType typeOfLayer[GEOLOGY_N_LAYER] = {Water,Shale,Sandstone,Shale};
    for( int y=1; y<h; ++y ) {
        int i = IofY(y);
        for( int j=0; j<w>>2; ++j ) {
            unsigned packed = 0;
            for( int k=0; k<4; ++k ) { 
                // It is "y-1" here because the geology indices run from 0 to h-1
                GeologyLayer layer = g.layer(j*4+k,y-1);
                unsigned r=typeOfLayer[layer];
                packed += r<<(2*k);
            }
            RockMap[i][j] = (unsigned char)packed;
        }
    }
}

//! Initialize wave field arrays.
static void InitializeFDTD() {
    int h = WavefieldHeight;
    int w = WavefieldWidth;

    // Vy at surface must be cleared because it is used for surface boundary condition.
    for( int j=0; j<w; ++j ) {
        Vy[0][j] = 0;
        Assert(Vx[0][j]==0);
        Assert(U[0][j]==0);
    }

    // Clear the FTDT fields.  The initial value for U is a bit of noise that 
    // prevents performance losses from denormal floating-point values.
    for( int y=0; y<h-1; ++y ) {
        int i = IofY(y);
        for( int j=0; j<w; ++j ) {
            int r = RockMap[i][j>>2]>>(2*(j&3))&3;
            // Store M/2 in A, because we sum two A values to compute an average M.
            A[i][j] = MofRock[r]*0.5f;
            B[i][j] = LofRock[r]; 
            U[i][j] = sinf(i*.1f)*cosf(j*.1f)*1.E-6;
            Vx[i][j] = 0;
            Vy[i][j] = 0;
        }
    }
}

static inline float SigmaRamp( float k ) {
    const float kMax = (DampSize-0.5f);
    const float sigmaMax = .3f;
    Assert(k<=kMax);
    // Bring it on quadratically
    float s = k*k*(sigmaMax/(kMax*kMax));
    Assert(s<=sigmaMax);
    return s;
}

//! Initialize PML-related fields and coefficients.
static void InitializePML() {
    int h = WavefieldHeight;
    int w = WavefieldWidth;

   // Clear left and right PML "psi".
    for( int y=0; y<h-1; ++y ) {
        int i = IofY(y);
        for( int j=0; j<DampSize; ++j )
            Pl[i][j] = Pr[i][j] = 0;
    }

    // Clear bottom PML "psi".
    for( int k=0; k<DampSize; ++k )
        for( int j=0; j<w; ++j )
            Pb[k][j] = 0;

    // Initialize UPML coefficients
    for( int k=0; k<DampSize; ++k ) {
        float s0 = SigmaRamp(k);
        float s1 = SigmaRamp(k+0.5f);
        D0[k] = (2-s0)/(2+s0);
        D1[k] = (2-s1)/(2+s1);
        D2[k] = 2/(2+s0);
        D3[k] = 2/(2+s1);
        D4[k] = s0;
        D5[k] = s1;
    }
}

static void ReplicateZone( int p, bool all ) {
    int w = WavefieldWidth;
    Assert(w>0);
    Assert(NumPanel>0);
    for( int k=0; k<PanelTransferCount; ++k ) {
        int i0 = PanelTransfer[p][k].srcI;
        int i1 = PanelTransfer[p][k].dstI;
        if( all ) {
            memcpy( A[i1], A[i0], w*sizeof(float) );
            memcpy( B[i1], B[i0], w*sizeof(float) );
        }
        memcpy( U[i1], U[i0], w*sizeof(float) );
        memcpy( Vx[i1], Vx[i0], w*sizeof(float) );
        memcpy( Vy[i1], Vy[i0], w*sizeof(float) );
        memcpy( Pl[i1], Pl[i0], DampSize*sizeof(float) );
        memcpy( Pr[i1], Pr[i0], DampSize*sizeof(float) );
    }
}

enum TileTag {
#if OPTIMIZE_HOMOGENEOUS_TILES
    TT_HomogeneousInterior,
#endif /* OPTIMIZE_HOMOGENEOUS_TILES */
    TT_HeterogeneousInterior,
    TT_Top,
    TT_Left,
    TT_Right,
    TT_BottomLeft,
    TT_Bottom,
    TT_BottomRight,
    TT_NumTileTag
};

struct Tile {
#if ALLOW_BIG_TILES
    unsigned tag:3;
    unsigned iFirst:10;
    unsigned iLen:10;
    unsigned jFirstOver4:9;
    unsigned jLenOver4:9;
#else
    unsigned tag:3;
    unsigned iFirst:10;
    unsigned iLen:4;
    unsigned jFirstOver4:9;
    unsigned jLenOver4:6;
#endif
};

// Fixme - estimate is way to high in practice, and theoretically too low
static const int NUM_TILE_MAX = WavefieldWidthMax*WavefieldHeightMax*PUMP_FACTOR_MAX;

static const Tile* PanelFirstTile[NUM_PANEL_MAX];
static const Tile* PanelLastTile[NUM_PANEL_MAX];
static Tile TileArray[NUM_TILE_MAX];
static Tile* TileEnd;

static inline TileTag Classify( int i, int j ) {
    Assert(1<=TopIofBottomRegion);
    Assert(DampSize<=LeftJofRightRegion);
    static const TileTag matrix[3][3] = {
        {TT_NumTileTag,TT_Top,                  TT_NumTileTag},
        {TT_Left,      TT_HeterogeneousInterior,TT_Right},
        {TT_BottomLeft,TT_Bottom,               TT_BottomRight}
    };
    return matrix[(1<=i)+(TopIofBottomRegion<=i)][(DampSize<=j)+(LeftJofRightRegion<=j)];
}

#if ASSERTIONS
static void CheckTiles( int p ) {
    Assert(sizeof(Tile)==4);
    static unsigned char TileDepth[WavefieldHeightMax][WavefieldWidthMax];
    int i0 = TrapezoidFirstI(p,0); 
    int i1 = TrapezoidLastI(p,0);
    for( int i=i0; i<i1; ++i )
        for( int j=0; j<WavefieldWidth; ++j )
            TileDepth[i][j]=0;
    const Tile* tFirst = PanelFirstTile[p];
    const Tile* tLast = PanelLastTile[p];
    for( const Tile* ptr=tFirst; ptr<tLast; ++ptr ) {
        Tile t = *ptr;
        int iFirst = t.iFirst;
        int iLast = iFirst+t.iLen;
        int jFirst = t.jFirstOver4*4;
        int jLast = jFirst+t.jLenOver4*4;
        Assert( i0<=iFirst );
        Assert( iLast<=i1 );
        Assert( 0<=jFirst );
        Assert( jLast<=WavefieldWidth );
        for( int i=iFirst; i<iLast; ++i ) {
            int j0 = i==0 ? DampSize : 0;
            int j1 = i==0 ? WavefieldWidth-DampSize : WavefieldWidth;
            for( int j=jFirst; j<jLast; ++j ) { 
                Assert( Classify(i,j)<TT_NumTileTag );
                int d = TileDepth[i][j];
                Assert( d<PumpFactor );
                Assert( TrapezoidFirstI(p,d)<=i );
                Assert( i<TrapezoidLastI(p,d) );
                if( TrapezoidFirstI(p,d)<i ) 
                    Assert( i-1==0 || TileDepth[i-1][j]==d+1 );
                if( i+1<TrapezoidLastI(p,d) )
                    Assert(TileDepth[i+1][j]==d);
                if( j0<j )
                    Assert(TileDepth[i][j-1]==d+1);
                if( j+1<j1 ) 
                    Assert(d==TileDepth[i][j+1]);
                TileDepth[i][j] = d+1;
            }
        }
    }
    for( int i=i0; i<i1; ++i ) {
        // Compute correct depth of trapezoid
        int depth = 0;
        for( int k=0; k<PumpFactor; ++k )
            if( TrapezoidFirstI(p,k)<=i && i<TrapezoidLastI(p,k) )
                ++depth;
        // Check that trapezoid is tiled to correct depth.
        for( int j=0; j<WavefieldWidth; ++j ) 
            if( i!=0 || (DampSize<=j && j<WavefieldWidth-DampSize ) )
                Assert( TileDepth[i][j]==depth );
    }
}
#endif /* ASSERTIONS */

static bool IsHomogeneous( int iFirst, int iLast, int jFirst, int jLast ) {
    // Load values that are used for a homogenous tile.
    float a = A[iFirst][jFirst];
    float b = B[iFirst][jFirst];
    for( int i=iFirst; i<iLast; ++i )
        for( int j=jFirst; j<jLast; ++j )
            // Check that values that would be loaded for a heterogenous tile match.
            if( A[i][j]!=a || A[i+1][j]!=a || A[i][j+1]!=a || B[i][j]!=b )
                return false;
    return true;
}

static void AddTile( int iFirst, int iLast, int jFirst, int jLast ) {
    // Caller is responsible for ensuring that tile is non-empty.
    Assert( iFirst<iLast );
    Assert( jFirst<jLast );
    // Caller is responsible for ensuring that tile is in bounds
    Assert( 0<=iFirst && iLast<=TopIofBottomRegion+DampSize );
    Assert( 0<=jFirst && jLast<=WavefieldWidth );
    TileTag tag = Classify(iFirst,jFirst);
#if ASSERTIONS
    // Caller is responsible for ensuring that tile has uniform tag.
    for( int i=iFirst; i<iLast; ++i )
        for( int j=jFirst; j<jLast; ++j )
            Assert(tag==Classify(i,j));
#endif /* ASSERTIONS */
    if( tag==TT_NumTileTag ) return;
#if OPTIMIZE_HOMOGENEOUS_TILES
    if( tag==TT_HeterogeneousInterior )
        if( IsHomogeneous( iFirst, iLast, jFirst, jLast ) )
            tag = TT_HomogeneousInterior;
#endif /* OPTIMIZE_HOMOGENEOUS_TILES */
    Tile t;
    t.tag=tag;
    Assert(int(t.tag)==tag);
    t.iFirst=iFirst;
    Assert(t.iFirst==iFirst);
    t.iLen=iLast-iFirst;
    Assert(t.iFirst+t.iLen==iLast);
    t.jFirstOver4=jFirst/4;
    Assert(t.jFirstOver4*4==jFirst);
    t.jLenOver4=(jLast-jFirst)/4;
    Assert(4*t.jFirstOver4+4*t.jLenOver4==jLast);
    Assert( TileEnd<TileArray+NUM_TILE_MAX );
    *TileEnd++ = t;
}

static void SplitHorizontal( int iFirst, int iLast, int jFirst, int jLast ) {
    Assert( iFirst<iLast );
    Assert( jFirst<jLast );
    if( jFirst<DampSize && DampSize<jLast ) {
        AddTile( iFirst, iLast, jFirst, DampSize );
        AddTile( iFirst, iLast, DampSize, jLast );
    } else if( jFirst<LeftJofRightRegion && LeftJofRightRegion<jLast ) {
        AddTile( iFirst, iLast, jFirst, LeftJofRightRegion );
        AddTile( iFirst, iLast, LeftJofRightRegion, jLast );
    } else {
        AddTile( iFirst, iLast, jFirst, jLast );
    }
}

static void SplitVertical( int iFirst, int iLast, int jFirst, int jLast ) {
    Assert( DampSize<=TopIofBottomRegion ); 
    if( iFirst<iLast && jFirst<jLast ) {
        if( iFirst<1 && 1<iLast ) {
            SplitHorizontal( iFirst, 1, jFirst, jLast );
            SplitHorizontal( 1, iLast, jFirst, jLast );
        } else if( iFirst<TopIofBottomRegion && TopIofBottomRegion<iLast ) {
            SplitHorizontal( iFirst, TopIofBottomRegion, jFirst, jLast );
            SplitHorizontal( TopIofBottomRegion, iLast, jFirst, jLast );
        } else {
            SplitHorizontal( iFirst, iLast, jFirst, jLast );
        }
    }
}

void MakeTilesForPanel( int p ) {
    Assert(TileWidth%4==0);
    PanelFirstTile[p] = TileEnd;
    int w = WavefieldWidth;
    int d = PumpFactor-1;
    int i0=TrapezoidFirstI(p,0);
    int i1=TrapezoidLastI(p,0);
    for( int i=i0; i-d<i1; i+=TileHeight ) 
        for( int j=0; j-4*d<w; j+=TileWidth )
            for( int k=0; k<=d; ++k )
                SplitVertical( Max(i-k,TrapezoidFirstI(p,k)), Min(i-k+TileHeight,TrapezoidLastI(p,k)),
                               Max(j-4*k,0), Min(j-4*k+TileWidth,w) );
    PanelLastTile[p] = TileEnd;
#if ASSERTIONS
    CheckTiles(p);
#endif /* ASSERTIONS */
}

static void InitializeTiles() {
    TileEnd=TileArray;
    for( int p=0; p<NumPanel; ++p ) {
        int yFirst=PanelFirstY[p];
        int yLast=PanelFirstY[p+1];
        if( p>0 ) yFirst-=PumpFactor;
        if( p+1<NumPanel ) yLast+=PumpFactor-1;
        MakeTilesForPanel(p);
    }
}

void WavefieldInitialize( const Geology& g ) {
    WavefieldHeight = g.height()+1;
    WavefieldWidth = g.width();
    InitializePanelMap();
    InitializeRockMap(g);
    InitializeFDTD();
    InitializePML();
    InitializeZoneTranfers();
    for( int p=1; p<NumPanel; ++p )
        ReplicateZone(p,/*all=*/true);
    InitializeTiles();
}

int WavefieldGetPumpFactor() {return PumpFactor;}

void WavefieldSetPumpFactor( int d ) {
    Assert(1<=d && d<=PUMP_FACTOR_MAX);
    if( PumpFactor!=d ) {
        PumpFactor = d;
        InitializeZoneTranfers();
        for( int p=1; p<NumPanel; ++p )
            ReplicateZone(p,/*all=*/true);
        InitializeTiles();  
    }
}

static int AirgunY, AirgunX;
static float AirgunImpulseValue[PUMP_FACTOR_MAX];
static int AirgunImpulseCounter[NUM_PANEL_MAX];

void WavefieldSetImpulseLocation( int x, int y ) {
    AirgunY = y;
    AirgunX = x;
}

#if USE_SSE
#define CAST(x) (*(__m128*)&(x))        /* for aligned load or store */
#define LOAD(x) _mm_loadu_ps(&(x))      /* for unaligned load */
#define ADD _mm_add_ps
#define MUL _mm_mul_ps
#define SUB _mm_sub_ps
#endif /* USE_SSE */

static void WavefieldUpdatePanel( int p ) {
    const int topIofBottomRegion = TopIofBottomRegion;
    const int leftJofRightRegion = LeftJofRightRegion;
    const int airgunJ = AirgunX+HIDDEN_BORDER_SIZE;
    const int airgunI = (AirgunY-PanelFirstY[p])+PanelFirstI[p];
    const Tile* tFirst = PanelFirstTile[p];
    const Tile* tLast = PanelLastTile[p];
    for( const Tile* ptr=tFirst; ptr<tLast; ++ptr ) {
        Tile t = *ptr;
        int iFirst = t.iFirst;
        int iLast = iFirst+t.iLen;
        int jFirst = t.jFirstOver4*4;
        int jLast = jFirst+t.jLenOver4*4;
        switch( t.tag ) {
            case TT_Top:
                // Reflection boundary condition at top.
                Assert(iFirst==0);
                Assert(iLast==1);
                for( int j=jFirst; j<jLast; ++j ) {
                    Assert(Vx[0][j]==0);
                    Assert(U[0][j]==0);
                    Vy[0][j] += 4*A[1][j]*(U[1][j]/*-U[0][j]*/);
                }
                break;
            case TT_Left: 
                // Left border
                for( int i=iFirst; i<iLast; ++i ) {
                    for( int j=jFirst, l=DampSize-1-jFirst; j<jLast; ++j, --l ) {
                        // Uniaxial PML along X axis
                        float u = U[i][j];      
                        Vx[i][j] = D0[l]*Vx[i][j]+D2[l]*(A[i][j+1]+A[i][j])*(U[i][j+1]-u);  
                        Vy[i][j] =       Vy[i][j]+      (A[i+1][j]+A[i][j])*(U[i+1][j]-u);
                        U [i][j] = D1[l]*u       +B[i][j]*(D3[l]*((Vx[i][j]-Vx[i][j-1])+Pl[i][l]) + (Vy[i][j]-Vy[i-1][j])); 
                        Pl[i][l] = D6   *Pl[i][l]+         D5[l]*                                   (Vy[i][j]-Vy[i-1][j]);
                    }
                }
                break;
#if OPTIMIZE_HOMOGENEOUS_TILES
            case TT_HomogeneousInterior:  {
                // Interior
#if USE_ARRAY_NOTATION
                // Array notation form - readable and fast
                size_t m = iLast-iFirst;
                size_t n = jLast-jFirst;
                int i = iFirst;
                int j = jFirst;
                float a = 2*A[i][j];
                float b = B[i][j];
                Vx[i:m][j:n] += a*(U[i:m][j+1:n]-U[i:m][j:n]);
                Vy[i:m][j:n] += a*(U[i+1:m][j:n]-U[i:m][j:n]);
                U[i:m][j:n] += b*((Vx[i:m][j:n]-Vx[i:m][j-1:n])+(Vy[i:m][j:n]-Vy[i-1:m][j:n]));
#elif USE_SSE
                // SSE form - less readable but fast
                __m128 a = CAST(A[iFirst][jFirst]);
                a = ADD(a,a);
                __m128 b = CAST(B[iFirst][jFirst]); 
                for( int i=iFirst; i<iLast; ++i ) {
                    // Fissioning inner loop into two loops seems to get best performance from Core-2 processors.
                    for( int j=jFirst; j<jLast; j+=4 ) {  
                        CAST(Vx[i][j]) = ADD(CAST(Vx[i][j]),MUL(a,SUB(LOAD(U[i][j+1]),CAST(U[i][j]))));
                        CAST(Vy[i][j]) = ADD(CAST(Vy[i][j]),MUL(a,SUB(CAST(U[i+1][j]),CAST(U[i][j])))); 
                    }
                    for( int j=jFirst; j<jLast; j+=4 ) 
                        CAST(U[i][j]) = ADD(CAST(U[i][j]),MUL(b,ADD(SUB(CAST(Vx[i][j]),LOAD(Vx[i][j-1])),SUB(CAST(Vy[i][j]),CAST(Vy[i-1][j])))));   
                }
#else
                // Scalar form - more readable but slow
                float a = 2*A[iFirst][jFirst];
                float b = B[iFirst][jFirst];
                for( int i=iFirst; i<iLast; ++i ) {
                    for( int j=jFirst; j<jLast; ++j ) {
                        Vx[i][j] += a*(U[i][j+1]-U[i][j]);
                        Vy[i][j] += a*(U[i+1][j]-U[i][j]);
                        U[i][j] += b*((Vx[i][j]-Vx[i][j-1])+(Vy[i][j]-Vy[i-1][j]));
                    }
                }
#endif           
                break;
            }
#endif /* OPTIMIZE_HOMOGENEOUS_TILES */
            case TT_HeterogeneousInterior: {
                // Interior
#if USE_ARRAY_NOTATION
                // Array notation form - readable and fast
                size_t m = iLast-iFirst;
                size_t n = jLast-jFirst;
                int i = iFirst;
                int j = jFirst;
                Vx[i:m][j:n] += (A[i:m][j:n]+A[i:m][j+1:n])*(U[i:m][j+1:n]-U[i:m][j:n]);
                Vy[i:m][j:n] += (A[i:m][j:n]+A[i+1:m][j:n])*(U[i+1:m][j:n]-U[i:m][j:n]);
                U[i:m][j:n] += B[i:m][j:n]*((Vx[i:m][j:n]-Vx[i:m][j-1:n])+(Vy[i:m][j:n]-Vy[i-1:m][j:n]));
#elif USE_SSE
                // SSE form - less readable but fast
                for( int i=iFirst; i<iLast; ++i ) {
                    for( int j=jFirst; j<jLast; j+=4 ) {
                        __m128 u = CAST(U[i][j]);   
                        __m128 a = CAST(A[i][j]); 
                        CAST(Vx[i][j]) = ADD(CAST(Vx[i][j]),MUL(ADD(LOAD(A[i][j+1]),a),SUB(LOAD(U[i][j+1]),u)));
                        CAST(Vy[i][j]) = ADD(CAST(Vy[i][j]),MUL(ADD(CAST(A[i+1][j]),a),SUB(CAST(U[i+1][j]),u)));    
                        CAST(U[i][j]) = ADD(u,MUL(CAST(B[i][j]),ADD(SUB(CAST(Vx[i][j]),LOAD(Vx[i][j-1])),SUB(CAST(Vy[i][j]),CAST(Vy[i-1][j])))));                           
                    }
                }
#else
                // Scalar form - more readable but slow
                for( int i=iFirst; i<iLast; ++i ) {
                    for( int j=jFirst; j<jLast; ++j ) {
                        float u = U[i][j];  
                        Vx[i][j] += (A[i][j+1]+A[i][j])*(U[i][j+1]-u);  
                        Vy[i][j] += (A[i+1][j]+A[i][j])*(U[i+1][j]-u);
                        U [i][j] = u + B[i][j]*((Vx[i][j]-Vx[i][j-1]) + (Vy[i][j]-Vy[i-1][j])); 
                    }
                }
#endif
                break;
            }
            case TT_Right:
                // PML region on right side.
                for( int i=iFirst; i<iLast; ++i ) {
                    for( int j=jFirst, l=j-leftJofRightRegion; j<jLast; ++j, ++l ) {
                        // Uniaxial PML along X axis
                        float u = U[i][j];      
                        Vx[i][j] = D1[l]*Vx[i][j]+D3[l]*(A[i][j+1]+A[i][j])*(U[i][j+1]-u);  
                        Vy[i][j] =       Vy[i][j]+      (A[i+1][j]+A[i][j])*(U[i+1][j]-u);
                        U [i][j] = D0[l]*u       +B[i][j]*(D2[l]*((Vx[i][j]-Vx[i][j-1])+Pr[i][l]) + (Vy[i][j]-Vy[i-1][j])); 
                        Pr[i][l] = D6   *Pr[i][l]+         D4[l]*                                   (Vy[i][j]-Vy[i-1][j]);
                    }
                }
                break;
            case TT_BottomLeft: 
                // PML region for bottom left corner.
                for( int i=iFirst, k=i-topIofBottomRegion; i<iLast; ++i, ++k ) {
                    for( int j=jFirst, l=DampSize-1-jFirst; j<jLast; ++j, --l ) {
                        Assert(0<=k && k<DampSize);
                        // Uniaxial PML along X and Y axis
                        float u = U[i][j];      
                        Vx[i][j] = D0[l]*Vx[i][j]+D2[l]*(A[i][j+1]+A[i][j])*(U[i][j+1]-u);  
                        Vy[i][j] = D1[k]*Vy[i][j]+D3[k]*(A[i+1][j]+A[i][j])*(U[i+1][j]-u);  
                        U [i][j] = D0[k]*D1[l]*u       +B[i][j]*(D3[l]*((Vx[i][j]-Vx[i][j-1])+Pl[i][l]) + D2[k]*((Vy[i][j]-Vy[i-1][j])+Pb[k][j]));  
                        Pb[k][j] = D6   *Pb[k][j]+D4[k]*(Vx[i][j]-Vx[i][j-1]);
                        Pl[i][l] = D6   *Pl[i][l]+D5[l]*(Vy[i][j]-Vy[i-1][j]);                           
                    }
                }
                break;
            case TT_Bottom: 
                // PML region on bottom.
                for( int i=iFirst, k=i-topIofBottomRegion; i<iLast; ++i, ++k ) {
                    for( int j=jFirst; j<jLast; ++j ) {
                        Assert(0<=k && k<DampSize);
                        Assert(A[i][j]!=0);
                        // FIXME - does not damp very high frequencies.
                        // Uniaxial PML along Y axis
                        float u = U[i][j];      
                        Vx[i][j] =       Vx[i][j]+      (A[i][j+1]+A[i][j])*(U[i][j+1]-u);
                        Vy[i][j] = D1[k]*Vy[i][j]+D3[k]*(A[i+1][j]+A[i][j])*(U[i+1][j]-u);  
                        U [i][j] = D0[k]*u       +B[i][j]*((Vx[i][j]-Vx[i][j-1])+D2[k]*((Vy[i][j]-Vy[i-1][j])+Pb[k][j]));   
                        Pb[k][j] = D6   *Pb[k][j]+D4[k]*   (Vx[i][j]-Vx[i][j-1]);
                    }
                }
                break;
            case TT_BottomRight: {
                // PML region for bottom right corner.
                for( int i=iFirst, k=i-topIofBottomRegion; i<iLast; ++i, ++k ) {
                    for( int j=jFirst, l=j-leftJofRightRegion; j<jLast; ++j, ++l ) {
                        Assert(0<=k && k<DampSize);
                        // Uniaxial PML along X and Y axis
                        float u = U[i][j];      
                        Vx[i][j] = D1[l]*Vx[i][j]+D3[l]*(A[i][j+1]+A[i][j])*(U[i][j+1]-u);  
                        Vy[i][j] = D1[k]*Vy[i][j]+D3[k]*(A[i+1][j]+A[i][j])*(U[i+1][j]-u);  
                        U [i][j] = D0[k]*D0[l]*u       +B[i][j]*(D2[l]*((Vx[i][j]-Vx[i][j-1])+Pr[i][l]) + D2[k]*((Vy[i][j]-Vy[i-1][j])+Pb[k][j]));  
                        Pb[k][j] = D6   *Pb[k][j]+D4[k]*(Vx[i][j]-Vx[i][j-1]);
                        Pr[i][l] = D6   *Pr[i][l]+D4[l]*(Vy[i][j]-Vy[i-1][j]);          
                    }
                }
                break;
            }
        }
        if( iFirst<=airgunI && airgunI<iLast && jFirst<=airgunJ && airgunJ<jLast ) {
            Assert( 0<=AirgunImpulseCounter[p] && AirgunImpulseCounter[p]<PumpFactor );
            U[airgunI][airgunJ] += AirgunImpulseValue[AirgunImpulseCounter[p]++];
        } 
    }
}

void WavefieldCopySurface( float* output, int w ) {
    Assert(w==WavefieldWidth-2*HIDDEN_BORDER_SIZE);
    for( int j=HIDDEN_BORDER_SIZE; j<w+HIDDEN_BORDER_SIZE; ++j )
        *output++ = Vy[1][j];
}

static float WaveClutShowsGeology;
static float WaveClutShowsSeismic;
static ColorFunc WaveClutColorFunc;

static NimblePixel WaveClut[RockTypeMax+2][SAMPLE_CLUT_SIZE];

static void ComputeWaveClut( const NimblePixMap& map, float showGeology, float showSeismic, ColorFunc colorFunc ) {
    Assert(0<=showGeology && showGeology<=1);
    Assert(0<=showSeismic && showSeismic<=1);
    // Check against current CLUT
    if( showGeology==WaveClutShowsGeology && showSeismic==WaveClutShowsSeismic && colorFunc==WaveClutColorFunc ) 
        // Use existing CLUT
        return;
    
    // Need to recompute CLUT
    WaveClutShowsGeology = showGeology;
    WaveClutShowsSeismic = showSeismic;
    WaveClutColorFunc = colorFunc;
    for( int r=0; r<=RockTypeMax; ++r ) 
        ColorFuncMakeClut( WaveClut[r], r, map, showGeology, showSeismic, colorFunc ); 
}

static void WavefieldDrawPanel( int p, const NimblePixMap& map ) { 
    const int w = map.width();
    const int h = map.height();
    Assert( h>0 );
    Assert( w>=4 );
#if USE_SSE
    Assert( w%4==0 );
    const __m128 upperLimit=_mm_set_ps1(SAMPLE_CLUT_SIZE/2-1);
    const __m128 lowerLimit=_mm_set_ps1(-SAMPLE_CLUT_SIZE/2);
#else
    const float upperLimit=SAMPLE_CLUT_SIZE/2-1;
    const float lowerLimit=-SAMPLE_CLUT_SIZE/2;
#endif /* USE_SSE */

    const NimblePixel* clut = WaveClut[0]+SAMPLE_CLUT_SIZE/2;
    int firstY = Max(0,PanelFirstY[p]);
    int lastY = Min(PanelFirstY[p+1],h);
    for( int y=firstY; y<lastY; ++y ) {
        int i = IofY(y);
        const byte* rock = &RockMap[i][HIDDEN_BORDER_SIZE>>2];
#if USE_SSE
        const __m128* in = (__m128*)&U[i][HIDDEN_BORDER_SIZE];
#else
        const float* in = &U[i][HIDDEN_BORDER_SIZE];
#endif
        NimblePixel* out = (NimblePixel*)map.at(0,y);
#pragma ivdep
        for( int j=0; j<w; j+=4, ++rock ) {
            unsigned r = *rock<<SAMPLE_CLUT_LG_SIZE;
            const unsigned SUBCLUT_MASK = 3<<SAMPLE_CLUT_LG_SIZE;
#if USE_SSE 
            __m128 v = _mm_max_ps(_mm_min_ps(*in++,upperLimit),lowerLimit);
#define STEP(k) out[k] = clut[_mm_cvt_ss2si(_mm_shuffle_ps(v,v,k))+(r>>(2*k)&SUBCLUT_MASK)]
            STEP(0);
            STEP(1);
            STEP(2);
            STEP(3);
            out+=4;
#else
            for( int k=0; k<4; ++k ) {
                float v = *in++;
                if( v>upperLimit ) v=upperLimit;
                if( v<lowerLimit ) v=lowerLimit;
                int u=int(v)+(r&SUBCLUT_MASK);
                *out++ = clut[u];
                r>>=2;
            }
#endif
        }
    }
}

#if DRAW_TILES
static void WavefieldDrawTiles( int p, const NimblePixMap& map ) {
    const int h = WavefieldHeight;
    const int w = WavefieldWidth; 
    Assert(w==map.width()+2*HIDDEN_BORDER_SIZE);
    NimblePixel red = map.pixel( NimbleColor(255,0,0));
    NimblePixel yellow = map.pixel( NimbleColor(255,255,0));
    NimblePixel green = map.pixel( NimbleColor(0,255,0) );
    NimblePixel purple = map.pixel( NimbleColor(255,0,255) );

    for( int x=0; x<map.width(); x+=4 )
        *(NimblePixel*)map.at(x,PanelFirstY[1]) = purple;

    const Tile* tFirst = PanelFirstTile[p];
    const Tile* tLast = PanelLastTile[p];
    for( const Tile* ptr=tFirst; ptr<tLast; ++ptr ) {
        Tile t = *ptr;
        int iFirst = t.iFirst;
        int iLast = iFirst+t.iLen;
        int jFirst = t.jFirstOver4*4;
        int jLast = jFirst+t.jLenOver4*4;
        int yFirst=PanelFirstY[p]+(iFirst-PanelFirstI[p]);
        int yLast=PanelFirstY[p]+(iLast-PanelFirstI[p]);
        int xFirst=jFirst-HIDDEN_BORDER_SIZE;
        int xLast=jLast-HIDDEN_BORDER_SIZE;
        if( 0<=yFirst && yLast<=map.height() )
            if( 0<=xFirst && xLast<=map.width() ) {
                NimblePixel pixel;
                switch( t.tag ) {
                    default:
                        pixel = red;
                        break;
#if OPTIMIZE_HOMOGENEOUS_TILES
                    case TT_HomogeneousInterior:
                        pixel = green;
                        break;
#endif /* OPTIMIZE_HOMOGENEOUS_TILES */
                    case TT_HeterogeneousInterior:
                        pixel = yellow;
                }
                for( int x=xFirst; x<xLast; ++x )
                    *(NimblePixel*)map.at(x,yFirst) = pixel;
                for( int y=yFirst; y<yLast; ++y )
                    *(NimblePixel*)map.at(xFirst,y) = pixel;
            }
    }
}
#endif /* DRAW_TILES */

//! Operations required by parallel_ghost_cell template.
class UpdateOps {
    const NimblePixMap& map; 
    const NimbleRequest request;
public:
    void exchangeBorders( int p ) const {
        Assert(0<p);
        Assert(p<NumPanel);
        ReplicateZone(p,/*all=*/false);
    }

    void updateInterior( int p ) const {
        if( request&NimbleUpdate ) 
            WavefieldUpdatePanel( p );    
        if( request&NimbleDraw ) { 
            WavefieldDrawPanel( p, map );
#if DRAW_TILES
            WavefieldDrawTiles( p, map );
#endif /* DRAW_TILES */
        }
    }
    UpdateOps( const NimblePixMap& map_, NimbleRequest request_ ) : map(map_), request(request_) {}
};

#if DRAW_COLOR_SCALE
static void DrawColorScale( const NimblePixMap& map ) {
    int xScale = 3;
    int xLimit = Min(SAMPLE_CLUT_SIZE/xScale,map.width());
    for( int y=0; y<24; ++y ) {
        int i = y/6;
        for( int x=0; x<xLimit; ++x ) 
            *(NimblePixel*)map.at(x,y) = WaveClut[i][x*xScale];
    }
}
#endif /* DRAW_COLOR_SCALE */

void WavefieldUpdateDraw( const NimblePixMap& map, NimbleRequest request, float showGeology, float showSeismic, ColorFunc colorFunc ) {
    ComputeWaveClut( map, showGeology, showSeismic, colorFunc );
    if( request&NimbleUpdate ) {
        int i = IofY(AirgunY);
        int j = AirgunX+HIDDEN_BORDER_SIZE;
        float a = A[i][j];
        for( int k=0; k<PumpFactor; ++k )
            AirgunImpulseValue[k] = AirgunGetImpulse( a );
        for( int p=0; p<NumPanel; ++p )
            AirgunImpulseCounter[p] = 0;
    }
    UpdateOps g(map,request);
    parallel_ghost_cell(NumPanel,g);
#if DRAW_COLOR_SCALE
    if( request&NimbleDraw )
        DrawColorScale(map);
#endif /* DRAW_COLOR_SCALE */
}
