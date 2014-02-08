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
 Airgun physics for Seismic Duck
*******************************************************************************/

//! Airgun signatures
enum AirgunPulseKind {
    APK_square,
    APK_gaussian,
    APK_gaussian_slope,
    APK_ricker
};

const AirgunPulseKind APK_N_SIGNATURE = AirgunPulseKind(APK_ricker+1);

class AirgunParameters {
public:
    AirgunPulseKind pulseKind;
    float frequency;
    float amplitude;
    AirgunParameters() :
        pulseKind(APK_gaussian),
        frequency(1.0f),
        amplitude(1.0f)
    {}
};

void AirgunInitialize( const AirgunParameters& parameters );
void AirgunFire( int x, int y );
float AirgunGetImpulse( float rockFactor );

class GraphMeter;
extern GraphMeter AirgunMeter;
