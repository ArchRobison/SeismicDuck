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
 Reservoir model for Seismic Duck
*******************************************************************************/

#include "Config.h"
#include "NimbleDraw.h"
#include "Geology.h"
#include "Reservoir.h"
#include "Utility.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <limits.h>

// Note: Reservoir solver becomes unstable if sum of permeabilities exceeds 1/2.
const float HorizontalPermeability = 0.4f;
const float VerticalPermeability = 0.1f;

struct ReservoirCell {
    //! Saturation of phase.
    float saturation[N_Phase];
     
    //! Pressure.
    /** Computed as sum of saturations. */
    float pressure;
        
    //! Connection coefficients.
    float bottomInOut, rightInOut;
};

//! Nonzero if cell at [v][u] is porous (i.e. can hold fluids)
/** Bits within the byte correspond to individual pixels. 
        0 1
        2 3
  */
static byte IsPorous[RESERVOIR_V_MAX][RESERVOIR_U_MAX];

static ReservoirCell Cell[RESERVOIR_V_MAX][RESERVOIR_U_MAX];

//! Width of reservoir (in cells)
static int ReservoirWidth;

//! Height of reservoir (in cells)
static int ReservoirHeight;

struct RunItem {    
    //! v coordinate of first cell in run.  
    unsigned v: 10;
    //! u coordinate of first cell in run
    unsigned ubegin: 10;    
    //! u coordinate of one past last cell in run
    unsigned uend: 10;
};

//! Space for run-length encoding used for drawing the reservoir.
static RunItem RunSet[RESERVOIR_V_MAX*GEOLOGY_NBUMP_MAX];

//! Pointer to one past last valid item in RunSet.
static RunItem* RunSetEnd;

//------------------------------------------------------------------------

static inline int UofX( int x ) {
    return (x+HIDDEN_BORDER_SIZE)/RESERVOIR_SCALE;
}

static inline int VofY( int y ) {
    return y/RESERVOIR_SCALE;
}

static void FindPorousCells( const Geology& g ) {
    const int xWidth = g.width();
    const int yHeight = g.height();
    const int uWidth = xWidth/RESERVOIR_SCALE;
    const int vHeight = yHeight/RESERVOIR_SCALE;
    Assert( uWidth<=RESERVOIR_U_MAX );
    Assert( vHeight<=RESERVOIR_V_MAX );
    // Initialize IsPorous to be non-porous everywhere.
    for( int v=0; v<vHeight; ++v ) {
        for( int u=0; u<uWidth; ++u ) {
            IsPorous[v][0] = 0;
            IsPorous[v][uWidth-1] = 0;
        }
    }
    // Now account for porous portions
    // The bitmask logic below works only when RESERVOIR_SCALE==2.
    Assert( RESERVOIR_SCALE==2 );
    for( int y=0; y<yHeight; ++y ) {
        int v = y/RESERVOIR_SCALE;
        for( int x=0; x<xWidth; ++x ) {
            int u = x/RESERVOIR_SCALE;
            if( g.layer(x,y)==MIDDLE_SANDSTONE )
                IsPorous[v][u]|=1<<((y&1)*2+(x&1));
        }
    }
}

static void MakeRunSet( int uWidth, int vHeight ) {
    RunItem* item = RunSet;
    int ubegin = 0;
    int uend = uWidth;
    for( int v=0; v<vHeight; v++ ) {
        int u = ubegin;
        while( u<uend ) {
            // Find porous
            while( !IsPorous[v][u] )
                if( ++u==uend ) 
                    goto nextRow;
            // Found beginning of a row of porous cells.
            item->v = v;
            item->ubegin = u;
            while( ++u<uend && IsPorous[v][u] )
                continue;
            item->uend = u;
            item++;
            Assert( size_t(item-RunSet) < sizeof(RunSet)/sizeof(RunSet[0]) );
        }
nextRow:;
    }
    // Failure of following assertion indicates that isPorous was not set.
    Assert(item!=RunSet||STUDY_DAMPING);
    RunSetEnd = item;
}

void ClearCells( int uWidth, int vHeight) {
    for( int v=0; v<vHeight; ++v ) {
        std::memset( IsPorous[v], 0, uWidth*sizeof(IsPorous[v][0]) );
        std::memset( Cell[v], 0, uWidth*sizeof(Cell[v][0]) );
    }
}

struct Hill {
    //! v coordinate of top of hill
    short vTop;
    //! v coordinate of bottom of hill (one past last fillable row of cells)
    short vBottom;
    //! u coordinates of left extreme of hill
    short uLeft;
    //! u coordinate right exteme of hill (one past last fillable column)
    short uRight;
    //! Maximum fluid capacity of the hill
    int volume;
};

static void FillPorousCells( ReservoirStats& s, int uWidth, int vHeight ) {
    s.numTrap=0;
    s.volume[OIL]=0;
    s.volume[GAS]=0;
    short fluidTop[RESERVOIR_U_MAX+1];
    short fluidBottom[RESERVOIR_U_MAX+1];
    // Find top and bottom porous cell in each column.
    for( int u=0; u<uWidth; ++u ) {
        int v;
        for( v=0; v<vHeight && !IsPorous[v][u]; v++ ) 
            continue;
        fluidTop[u] = v;
        for( ; v<vHeight && IsPorous[v][u]; ++v ) {
             // Fill column with water.  
            for( int k=0; k<N_Phase; ++k )
                Cell[v][u].saturation[k] = k==WATER ? 1.f : 0.f;
            Cell[v][u].pressure = 1.0f;
            Cell[v][u].rightInOut = IsPorous[v][u+1] ? HorizontalPermeability : 0;
            Cell[v][u].bottomInOut = IsPorous[v+1][u]? VerticalPermeability : 0;
        }
        fluidBottom[u] = v;
    }
    int infinity = SHRT_MAX;
    fluidTop[uWidth] = infinity;
    const int MAX_HILLS=RESERVOIR_U_MAX;
    Hill hill[MAX_HILLS];
    Hill* h = hill;
    int vLast = infinity; 
    for( int u=0;;) {
        // Skip flat part
        while( fluidTop[u]==vLast ) 
            ++u;
        if( u>=uWidth ) 
            break;
        h->uLeft = u;
        // Climb hill
        while( fluidTop[u]<=vLast ) {
            vLast = fluidTop[u];
            ++u;
        }
        h->vTop=vLast;
        // Walk over flat top of hill and down
        int uRight = u;
        while( u<=uWidth && fluidTop[u]>=vLast ) {
            if( fluidTop[u]>vLast ) 
                uRight=u;
            vLast = fluidTop[u];
            ++u;
        }
        h->uRight = uRight; 
        Assert(h->uRight<=uWidth);
        if( h->uLeft==0 )
            if( h->uRight==uWidth )
                // Hill traverses entire screen
                h->vBottom = Max(fluidTop[0],fluidTop[uWidth-1]);
            else
                // Hill up against left side of screen
                h->vBottom = fluidTop[h->uRight-1];
        else 
            if( h->uRight==uWidth )
                // Hill up against right side of screen
                h->vBottom = fluidTop[h->uLeft];
            else
                // Hill up against neither side
                h->vBottom = Min(fluidTop[h->uLeft],fluidTop[h->uRight-1]);
        Assert( h->vTop<=h->vBottom );
        for(;;) {
            while( h->uLeft<h->uRight && fluidTop[h->uLeft]>h->vBottom ) 
                h->uLeft+=1;
            while( h->uLeft<h->uRight && fluidTop[h->uRight-1]>h->vBottom ) 
                h->uRight-=1;
            if( h->uRight-h->uLeft<=uWidth*0.2f || h->vBottom-h->vTop<=vHeight*.05f )
                break;
            // Trap is so wide and deep that it is too easy to find.
            // Raise bottom by one vertical unit and recheck.
            h->vBottom-=1;
        }
        ++h;
    }
    int totalVolume = 0;
    Hill* hEnd = h;
    // Compute volume of each trap
    for( Hill* h=hill; h<hEnd; ++h ) {
        h->volume = 0;
        // Compute max fluid volume of hill in units of reservoir cells
        for( int u=h->uLeft; u<h->uRight; ++u ) 
            h->volume += Max(0,Min<int>( fluidBottom[u], h->vBottom )-fluidTop[u]);
        Assert( h->volume>=0 );
        s.numTrap += h->volume>0;
        totalVolume += h->volume;
    }
    // FIXME - randomize these fractions a little
    float gasFrac = 0.5f;
    float oilFrac = 0.5f;
    // FIXME - scale MaxVolume based on screen ssize
    const int MaxVolume = 1000000;
    if( totalVolume>=MaxVolume ) {
        // Prorate it
        gasFrac *= float(MaxVolume)/totalVolume;
        oilFrac *= float(MaxVolume)/totalVolume;
    }
    // Now fill the traps with gas and oil
    for( Hill* h=hill; h<hEnd; ++h ) {
        int avail[N_Phase];
        avail[GAS] = h->volume*gasFrac;
        avail[OIL] = h->volume*oilFrac; 
        avail[WATER] = INT_MAX;
        int fillPhase = GAS;
        for( int v=h->vTop; v<h->vBottom; ++v ) {
            while( avail[fillPhase]<=0 ) 
                ++fillPhase;
            if( fillPhase==WATER ) {
                // Already default filled cells with water.
                break;
            }
            for( int u=h->uLeft; u<h->uRight; ++u ) {
                if( IsPorous[v][u] ) {
                    ReservoirCell& c = Cell[v][u];
                    c.saturation[WATER] = c.saturation[OIL] = c.saturation[GAS] = 0;
                    c.saturation[fillPhase] = 1.0f;
                    avail[fillPhase]-=1;
                    if( fillPhase<2 )
                        s.volume[fillPhase] += 1;
                }
            }
        }
    }
}

struct ReservoirHole {
    //! Horizontal position of hole
    short x;
    //! Depth of hole
    short depth;
};

const int HoleCountMax=20;
static int HoleCount=0;
static ReservoirHole* HoleCurrent=NULL;

static ReservoirHole Hole[HoleCountMax];

void ReservoirInitialize( ReservoirStats& s, const Geology& g ) {
    int uWidth = ReservoirWidth = g.width()/RESERVOIR_SCALE;
    int vHeight = ReservoirHeight = g.height()/RESERVOIR_SCALE;
    ClearCells(uWidth,vHeight);
    FindPorousCells(g);
    MakeRunSet( uWidth, vHeight );
    FillPorousCells( s, uWidth, vHeight );
    HoleCount = 0;
}

//! Precomputes smooth coefficients for fluid extraction calculation
class Smooth {
    static const int centerX = DRILL_DIAMETER*2;
    float array[1+2*centerX];
public:
    Smooth();
    float operator()( int dx ) const {
        Assert( -centerX<=dx && dx<=centerX );
        return array[dx+centerX];
    }
};

Smooth::Smooth() {
    float sharpness = 1.0f/(DRILL_DIAMETER*DRILL_DIAMETER);
    for( int dx=-centerX; dx<=centerX; ++dx )
        array[dx+centerX] = 0.2f*expf(-sharpness*(dx*dx));
}

static const Smooth TheSmooth;

static void UpdateExtract( float amount[N_Phase] ) {
    Assert(ReservoirWidth>0);
    Assert(ReservoirHeight>0);
    for( int i=0; i<HoleCount; ++i ) {
        int x = Hole[i].x;
        int umin = Max(UofX(x-DRILL_DIAMETER),0);
        int umax = Min(UofX(x+DRILL_DIAMETER),ReservoirWidth-1);
        int y = Hole[i].depth;
        int vmin = Max(0,TheGeology.layerBottomCell(TOP_SHALE,UofX(x)));
        int vmax = Min(VofY(y),TheGeology.layerBottomCell(MIDDLE_SANDSTONE,UofX(x)));
        for( int v=vmin; v<=vmax; ++v ) 
            for( int u=umin; u<=umax;++u ) {
                ReservoirCell& cell = Cell[v][u];
                float total = cell.saturation[GAS] + cell.saturation[OIL] + cell.saturation[WATER];
                // Take one half of fluid, maxing out at 0.25 total unit.
                int dx = (u*RESERVOIR_SCALE-HIDDEN_BORDER_SIZE+1)-x;
                float fraction = total*TheSmooth(dx);
                float pressure = 0;
                for( int k=0; k<N_Phase; k++ ) {
                    float d_amount = cell.saturation[k] * fraction;
                    amount[k] += d_amount;
                    Assert(amount[k]<=1E37);
                    pressure += (cell.saturation[k] -= d_amount);
                    Assert( amount[0]<=1E10 );
                }
                cell.pressure = pressure;
            }
    }
}

static void UpdateFluxesAndSaturations() {
    // Flux carried into left cell from current cell 
    static float leftDelta[N_Phase];

    // Flux carried into cell above from current cell 
    static float aboveDelta[RESERVOIR_U_MAX][N_Phase];

    // Loop over porous cells
    for( RunItem* run=RunSet; run!=RunSetEnd; ++run ) {
        int ubegin = run->ubegin;
        int uend = run->uend;
        int v = run->v;

        for( int u=ubegin; u!=uend; ++u ) {
            ReservoirCell& cell = Cell[v][u];
            ReservoirCell& below = Cell[v+1][u];
            ReservoirCell& right = Cell[v][u+1];
            // Compute flow into horizontal neighbor and vertical neighbor.
            float uFlow = (right.pressure-cell.pressure) * cell.rightInOut;
            float vFlow = (below.pressure-cell.pressure) * cell.bottomInOut;
            // Set sourceU to point to r or r_right, from whichever flow goes out.
            // Do likewise for vertical neighbor.
            const ReservoirCell& sourceU = uFlow>=0 ? right : cell;
            const ReservoirCell& sourceV = vFlow>=0 ? below : cell;
            // Compute deltas for saturations.
            cell.pressure =0;
            for( int k=0; k<N_Phase; k++ ) {
                float deltaSatU = uFlow*sourceU.saturation[k];
                float deltaSatV = vFlow*sourceV.saturation[k];
                // Following assertion is true because a scan line never ends in a porous cell.
                Assert( leftDelta[k]==0.f || Cell[v][u-1].rightInOut!=0.f );
                // Following assertion is true because if Cell[v-1][u].bottomInOut==0, then
                // the most recently processed porous cell int he column must have had
                // Cell[v-1][u].bottomInOut==0.
                Assert( aboveDelta[u][k]==0.f || Cell[v-1][u].bottomInOut!=0.f );
                cell.pressure += cell.saturation[k] += (deltaSatU - leftDelta[k]) + (deltaSatV - aboveDelta[u][k]);
                Assert(cell.saturation[k]>=0);
                aboveDelta[u][k] = deltaSatV;
                leftDelta[k] = deltaSatU;
            }
        }
    }
}

void ReservoirUpdate( float fluidExtracted[N_Phase] ) {
    // Update the reservoir
    for( int k=0; k<N_Phase; k++ ) 
        fluidExtracted[k]=0;
    for( int t=0; t<4; ++t ) {
        UpdateExtract( fluidExtracted );
        UpdateFluxesAndSaturations();
    }
}

 void ReservoirDraw( const NimblePixMap& map ) {
    Assert( RESERVOIR_SCALE==2 );
    const int uleft = HIDDEN_BORDER_SIZE/RESERVOIR_SCALE;
    const int uright = uleft + map.width()/RESERVOIR_SCALE;
    const int vbottom = map.height()/RESERVOIR_SCALE;
    NimblePixel* const origin = (NimblePixel*)map.at(0,0)-HIDDEN_BORDER_SIZE;
    const ptrdiff_t downDelta = map.bytesPerRow()/sizeof(NimblePixel);
    for( RunItem* run = RunSet; run!=RunSetEnd; ++run ) {
        Assert( size_t(run-RunSet)<sizeof(RunSet)/sizeof(RunSet[0]) );
        int v = run->v;
        // Clip bottom
        if( v>=vbottom ) 
            break;
        Assert( run->ubegin<run->uend );
        // Clip left and right
        int ubegin = Max(int(run->ubegin),uleft);
        int uend = Min(int(run->uend),uright);
        if( ubegin>=uend )
            continue;
        NimblePixel* dst = origin+ubegin*RESERVOIR_SCALE+v*downDelta*RESERVOIR_SCALE; 
        const byte* isPorous = &IsPorous[v][ubegin];
        const ReservoirCell* c = &Cell[v][ubegin];
        const ReservoirCell* d = &Cell[v][uend];
        do {
            int red = int(NimbleColor::FULL*c->saturation[GAS]);
            int green = int(NimbleColor::FULL*c->saturation[OIL]);
            int blue = int(NimbleColor::FULL*c->saturation[WATER]);
            // FIXME - probably should inline color->pixel calculation.
            NimblePixel p = map.pixel(NimbleColor(red,green,blue));
            // In 2x2 block, write to upper right and lower left corners
            if( *isPorous&2 ) dst[1] = p; 
            if( *isPorous&4 ) dst[downDelta] = p;
            dst+=2;
            ++isPorous;
        } while( ++c<d );
    }
}

int ReservoirStartHole( int x ) {
    HoleCurrent = NULL;
    if( HoleCount>0 ) {
        ReservoirHole* closest = Hole;
        int minDistance = abs(x-closest->x);
        for( ReservoirHole* h=Hole+1; h<Hole+HoleCount; ++h ) {
            int distance = abs(x-h->x);
            if( distance<minDistance ) {
                closest=h;
                minDistance = distance;
            }
        }
        const int fuzz = 3;
        if( minDistance<=fuzz ) {
            HoleCurrent = closest;
            return closest->x;
        }
    }
    if( HoleCount<HoleCountMax ) {
        ReservoirHole* h = HoleCurrent = &Hole[HoleCount++];
        h->x=x;
        h->depth = 0;
    }
    return x;
}

int ReservoirUpdateHole( int& y, int direction ) {
    Assert( direction!=0 );
    int cost=0;
    if( HoleCurrent && y+direction>HoleCurrent->depth ) {
        if( y+direction>=TheGeology.oceanFloor() ) {
            cost = (y+direction)-HoleCurrent->depth;
            HoleCurrent->depth = y+=direction;;
        } else {
            // Can move twice as fast down through water.
            HoleCurrent->depth = y+=2*direction;
        }
    } else {
        // When not cutting, drill moves three times as fast.
        y+=3*direction;
        if( y<0 ) y=0;
    }
    return cost;
}

void ReservoirDrawHoles( NimblePixMap& subsurface ) {
    for( int k=0; k<HoleCount; ++k ) {
        int x = Hole[k].x;
        int y = Hole[k].depth;
        TheGeology.drawHole( subsurface, x, y  );
    }
}
