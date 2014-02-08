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
 Resource layer for Seismic Duck
*******************************************************************************/

#include "AssertLib.h"
#include "BuiltFromResource.h"
#include "Host.h"

BuiltFromResourcePixMap::BuiltFromResourcePixMap( const char* resourceName_ ) : 
    myResourceName(resourceName_) 
{}

void BuiltFromResourcePixMap::loadAll() {
    StartupListItem<BuiltFromResourcePixMap>::forAll( HostLoadResource );
}
