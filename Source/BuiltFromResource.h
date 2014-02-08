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
 Resource layer for Seismic Duck
*******************************************************************************/

#pragma once
#ifndef BuiltFromResource_H
#define BuiltFromResource_H

#include "StartupList.h"

class NimblePixMap;
class HostResourceLoader;

class BuiltFromResourcePixMap: public StartupListItem<BuiltFromResourcePixMap> {
    const char* const myResourceName;
    void operator=( const BuiltFromResourcePixMap& );          //!< Deny assignment
    BuiltFromResourcePixMap( const BuiltFromResourcePixMap& ); //!< Deny copy construction
public:
    BuiltFromResourcePixMap( const char* resourceName_ );
    const char* resourceName() const {return myResourceName;}

    virtual void buildFrom( const NimblePixMap& map ) = 0;

    //! Load all resources.
    static void loadAll();
};

#endif /* BuiltFromResource_H */