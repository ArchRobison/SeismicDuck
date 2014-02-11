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
 Color transfer functions for Seismic Duck
*******************************************************************************/

#pragma once
#ifndef ColorFunc_H
#define ColorFunc_H

#include "Config.h"
#include "NimbleDraw.h"

//! Mapping of magnitudes onto colors
enum ColorFunc {
    CF_Linear,
    CF_Log,
    CF_SignOnly
};

void ColorFuncMakeClut( NimblePixel clut[SAMPLE_CLUT_SIZE], int rockType, const NimblePixMap& map, float showGeology, float showSeismic, ColorFunc colorFunc ); 

#endif /* ColorFunc_H */
