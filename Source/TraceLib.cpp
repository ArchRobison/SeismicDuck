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

#include "TraceLib.h"
#include <stdio.h>
#include <stdarg.h>

#if TRACING
static FILE * TraceFile;
static int TraceLines = 0;

void WriteToTraceFile( const char * filename, int line, const char * format, ... ) {
    if( !TraceFile ) {
        TraceFile = fopen("trace.txt","w");
    }
    if( TraceLines<1000 ) {
        ++TraceLines;
        fprintf( TraceFile, "%s(%d): ", filename, line );
        va_list ap;
        va_start( ap, format );
        vfprintf( TraceFile, format, ap );
        va_end( ap );
        fprintf( TraceFile, "\n" );
        fflush( TraceFile );
    }
}
#endif /* TRACING */
