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
 Assertion checking
*******************************************************************************/

#include "AssertLib.h"
#include <cassert>

#if ASSERTIONS
#if !USE_HOST_ASSERT
extern void AssertionFailure(const char* filename, int line, const char* expression) {
    {
        // It's often handy to set a breakpoint on the assertion below.
        assert(0);
    }
}
#endif /*!USE_HOST_ASSERT*/
#endif /*NO_ASSERTIONS*/
