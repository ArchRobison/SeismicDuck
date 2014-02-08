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
 Inclusion of SSE intrinsics.
*******************************************************************************/

#if defined(_MSC_VER)
#define USE_SSE (_M_IX86_FP>=1)
#elif (__INTEL_COMPILER||!__MINGW32__||10*__GNUC__==4+__GNUC_MINOR__>=45) && defined(__SSE__)
// Condition above avoids SSE for GCC on Windows prior to GCC 4.5, because of bug on Windows.
#define USE_SSE 1
#endif /*  */

#if USE_SSE
#include <xmmintrin.h>
#endif /* USE_SSE */
