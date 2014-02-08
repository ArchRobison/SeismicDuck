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
 Support for writing a trace file. 
*******************************************************************************/

#pragma once
#ifndef TraceLib_H
#define TraceLib_H

#define TRACING 0

// Clients should use the macros Trace0..Trace2.
// The number indicates the number of arguments after the format string.
// Examples:
//     Trace0("Entering atmosphere")
//     Trace1("Monkey count = %d",count);
//     Trace2("Coordinate is (%d,%d)",x,y);
#ifdef TRACING
// Use whatever value was supplied on command line.
#else
// Default value
#define TRACING 0   /* Turn on tracing */
#endif /* TRACING */

#if TRACING
extern void WriteToTraceFile( const char * filename, int line, const char * format, ... );
#define Trace0(fmt) WriteToTraceFile(__FILE__,__LINE__,fmt)
#define Trace1(fmt,x) WriteToTraceFile(__FILE__,__LINE__,fmt,x)
#define Tracd2(fmt,x,y) WriteToTraceFile(__FILE__,__LINE__,fmt,x,y)
#else
#define Trace0(fmt) /**/
#define Trace1(fmt,x) /**/
#define Trace2(fmt,y) /**/
#endif /*!TRACING*/
#endif /*TraceLib_H*/
