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
 Configuration parameters for Seismic Duck
*******************************************************************************/

#pragma once
#ifndef Config_H
#define Config_H

//! Minimum width of panel in pixels
const int PANEL_MIN_WIDTH = 192;

//! Maximum supported display width
/** Must be multiple of 4.  
    Seem to get slightly better performance if it is not a multiple of 16 */
const int DISPLAY_WIDTH_MAX = 1924;

//! Maximum supported display height.
const int DISPLAY_HEIGHT_MAX = 1440;

//! Minimum display width
const int DISPLAY_WIDTH_MIN = 1024;

//! Minimum display height
const int DISPLAY_HEIGHT_MIN = 768;

//! Maximum width of visible wavefield
const int WAVEFIELD_VISIBLE_WIDTH_MAX = DISPLAY_WIDTH_MAX-PANEL_MIN_WIDTH;

//! Maximum height of visible wavefield
const int WAVEFIELD_VISIBLE_HEIGHT_MAX = DISPLAY_HEIGHT_MAX/2;

//! Maximum width of seismogram
const int SEISMOGRAM_WIDTH_MAX = WAVEFIELD_VISIBLE_WIDTH_MAX;

//! Maximum height of seismogram
const int SEISMOGRAM_HEIGHT_MAX = WAVEFIELD_VISIBLE_HEIGHT_MAX;

//! Log2 of size of a color lookup array for converting samples to colors.
const int SAMPLE_CLUT_LG_SIZE = 10;

//! Size of a color lookup array for converting samples to colors. 
const int SAMPLE_CLUT_SIZE = 1<<SAMPLE_CLUT_LG_SIZE;

/** Normally false.  Set to true if doing damping studies. */
const bool STUDY_DAMPING = false;

//! Size of hidden border around visible wavefield and reservoir, in pixels.
/** Normally equal to DampSize as defined in Wavefield.cpp.  
    Set to zero to inspect behavior of damping. */
const int HIDDEN_BORDER_SIZE = STUDY_DAMPING ? 0 : 16;

//! Dimension of a reservoir cell in pixels.
const int RESERVOIR_SCALE = 2;

//! Radius of drill in pixels.
const int DRILL_DIAMETER = 9;

#endif /*Config_H*/