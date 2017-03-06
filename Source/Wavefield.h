/* Copyright 1996-2017 Arch D. Robison 

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

#include "ColorFunc.h"
#include "NimbleDraw.h"

class Geology;

enum RockType {
    Water,
    Sandstone,
    Shale,
    RockTypeMax = Shale
};

//! Initialize fields for wave simulation.
void WavefieldInitialize( const Geology& g );

//! Update the wavefield and/or draw it.
void WavefieldUpdateDraw( const NimblePixMap& map, NimbleRequest request, float showGeology, float showSeismic, ColorFunc colorFunc );

//! Copy values from surface of wavefield into out[].
/** w is the width of the visible portion of the wavefield. */
void WavefieldCopySurface( float* out, int w );

//! Coordinates (x,y) are in the coordinate system of the "map" argument to WavefieldUpdateDraw. 
void WavefieldSetImpulseLocation( int x, int y );

//! Get "pump factor", which is number of timesteps per video frame.
int WavefieldGetPumpFactor();

//! Set "pump factor".  Value should be in closed interval [d,PUMP_FACTOR_MAX]
void WavefieldSetPumpFactor( int d );
