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
 Utility functions for Seismic Duck
*******************************************************************************/

template<typename T>
inline T Min( T a, T b ) {
    return a<b ? a : b;
}

template<typename T>
inline T Max( T a, T b ) {
    return !(a<b) ? a : b;
}

//! Return x clipped to closed interval [a,b]
template<typename T>
inline T Clip( T a, T b, T x ) {
    Assert(!(b<a));
    return x<a ? a : b<x ? b : x;
}

template<typename T>
inline void Swap( T& x, T& y ) {
    T tmp(x);
    x = y;
    y = tmp;
}

inline int Round( float x ) {
    x += x<0 ? -0.5f : 0.5f;    // FIXME add SSE version
    return int(x);
}

class NoCopy {
    NoCopy( const NoCopy& );
    void operator=( const NoCopy& );
public:
    NoCopy() {}
};