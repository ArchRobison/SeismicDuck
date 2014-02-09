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
 Seismogram for Seismic Duck
*******************************************************************************/

enum SeismogramKind {
    SK_CONTINUOUS,
    SK_SQUIGGLE
};

void SeismogramUpdateDraw( const NimblePixMap& map, NimbleRequest request, ColorFunc colorFunc, bool autogain );
void SeismogramReset( int width, int height );
