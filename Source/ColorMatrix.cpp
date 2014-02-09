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
 Two dimensional color table.
*******************************************************************************/

#include "ColorMatrix.h"

ColorMatrix::ColorMatrix( const char* resourceName ) : 
    BuiltFromResourcePixMap(resourceName),
    myColors(NULL),
    myWidth(0),
    myHeight(0)
{}

void ColorMatrix::buildFrom( const NimblePixMap& map ) {
    Assert(!myColors);
    int w = myWidth = map.width();
    int h = myHeight = map.height();
    myColors = new NimbleColor[myWidth*myHeight];
    for( int i=0; i<h; ++i )
        for( int j=0; j<w; ++j )
            myColors[i*w+j] = map.color(j,i);
}


