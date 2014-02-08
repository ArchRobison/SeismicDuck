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
 Assertion checking
*******************************************************************************/

#pragma once
#ifndef Assert_H
#define Assert_H

// Setting ASSERTIONS to 0 turns off internal checking.
// Setting ASSERTIONS to 1 turns on internal checking.
// (i.e. much more thorough at the cost of being slower.
#ifdef ASSERTIONS
// Use whatever value was supplied on command line.
#else
#define ASSERTIONS 1    /* Turn on assertions if debugging */
#endif /* ASSERTIONS */

#define USE_HOST_ASSERT 0

#if ASSERTIONS
#if USE_HOST_ASSERT
#include <cassert>
#define Assert assert
#else
void AssertionFailure(const char* filename_, int line, const char* expression);
#define Assert(predicate) ((predicate)?(void)0:(AssertionFailure(__FILE__,__LINE__,#predicate)))
#endif
template<typename T> inline void Poison(T*& ptr) {ptr=(T*)-1;}
#else
#define Assert(predicate) ((void)0)
template<typename T> inline void Poison(T*&) {}
#endif /*!ASSERTIONS*/

#endif /*Assert_H*/