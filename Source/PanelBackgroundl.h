//******************************************************************************
// Panel for Seismic Duck
//
// Copyright 1996-2009 Arch D. Robison
//
// Each meter keeps track of a value, and allows the value to be displayed
// according to the meter's type.
//******************************************************************************

#include "BuiltFromResource.h"

class PanelBackground: BuiltFromResourcePixMap {
public:
	MeterBase( const char* resourceName ) : 
        BuiltFromResourcePixMap(resourceName), myValue(0) 
    {}
	void setValue( float value ) {myValue=value;}
	float value() const {return myValue;}
	float operator+=( float addend ) {return myValue+=addend;}
    /*override*/ void buildFrom( const NimblePixMap& map );
protected:
    NimblePixMap myPixMap;   
    float myValue;
private:
	MeterBase( const MeterBase& );
	void operator=( const MeterBase& );
};

// Meter that looks like rotating wheels (e.g.1970's gas pumps and water meters).
class CounterMeter: public MeterBase {
public:
	CounterMeter( const char* resourceName, int nDigit );
    void drawOn( NimblePixMap& map, int x, int y ) const;
    int width() const {return myPixMap.width()*myNdigit;}
	int height() const {return myPixMap.height()/12;}
private:
