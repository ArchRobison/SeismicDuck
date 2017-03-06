/* Copyright 1996-2015 Arch D. Robison 

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
 Various widgets for Seismic Duck
*******************************************************************************/

#include "BuiltFromResource.h"

class Widget: BuiltFromResourcePixMap {
public:
    Widget(const char* resourceName ) :
        BuiltFromResourcePixMap(resourceName) 
    {}
    using BuiltFromResourcePixMap::setResourceName;
protected:
    NimblePixMapWithOwnership myPixMap;   
    void buildFrom( const NimblePixMap& map ) override;
private:
    Widget( const Widget& ) = delete;
    void operator=( const Widget& ) = delete;
};

//! Image that can be stretched to fit a rectangle.
/** Stretching does not distort the borders. */
class RubberImage: public Widget {
public:
    RubberImage( const char* resourceName );
    void drawOn( NimblePixMap& map ) const {drawOn(map,0,map.height());}
    void drawOn( NimblePixMap& map, int top, int bottom ) const;
};

class Font: BuiltFromResourcePixMap {
public:
    Font( const char* resourceName ) : 
        BuiltFromResourcePixMap(resourceName)
    {}
    using BuiltFromResourcePixMap::setResourceName;

    ~Font() {delete[] storage;}
    /** Returns x coordinate that next character would have. */
    int drawOn( NimblePixMap& map, int x, int y, const char* s, NimblePixel ink );
    int height() const {return myHeight;}
    int width( const char* string ) const;
private:
    static const char charMin=32;
    static const char charMax=127;
    byte* storage = nullptr;
    byte myHeight = 0;
    unsigned short start[charMax-charMin+2];
    void buildFrom( const NimblePixMap& map ) override;
    Font( const Widget& );
    void operator=( const Font& ) = delete;
    static bool isBlankColumn( const NimblePixMap& map, int x );
};

//! Meter that looks like rotating wheels (e.g.1970's gas pumps and water meters).
class WheelMeter: public Widget {
public:
    //! resourceName is name of background resource with transparent boxes where the digits go.
    WheelMeter( const char* resourceName );
    void drawOn( NimblePixMap& map, int x, int y ) const;
    void setValue( float value ) {myValue=value;}
    float operator+=( float a ) {return myValue+=a;}
    float operator-=( float a ) {return myValue-=a;}
    float value() const {return myValue;}
    int width() const {return myPixMap.width();}
    int height() const {return myPixMap.height();}
protected:
    void buildFrom(const NimblePixMap& map) override;
private:
    static const int maxNdigit = 4;
    float myValue;
    signed char myNdigit;
    NimbleRect myDigitWindow[maxNdigit];
};

class DigitalMeter: public Widget {
public:
    /** nDigit is total number of digits, including decimal point.
        and the nDecimal digits after the point. */
    DigitalMeter( int nDigit, int nDecimal );
    void drawOn( NimblePixMap& map, int x, int y ) const;
    void setValue( float value ) {myValue=value;}
    float value() const {return myValue;}
    int width() const {return myPixMap.width()*myNdigit;}
    int height() const {return myPixMap.height()/13;}
private:
    float myValue;
    signed char myNdigit;
    signed char myNdecimal;
};

//! Bar meter, such as used for gas/oil/water meters.
/** Value of 1 is full, value of zero is empty. */
class BarMeter: public Widget {
public:
    BarMeter( const char* resourceName, bool isVertical=true );
    void drawOn( NimblePixMap& map, int x, int y ) const;
    int width() const {return myPixMap.width();}
    int height() const {return myPixMap.height();}
    void setValue( float value ) {myValue=value;}
private:
    float myValue;
    //! Pixmap for empty meter
    NimblePixMapWithOwnership myEmptyPixMap;
    char myIsVertical;  // 1 if meter is oriented vertically, 0 otherwise.
    void buildFrom(const NimblePixMap& map) override;
};

class GraphMeter {
public:
    GraphMeter( int width, int height, NimbleColor penColor = NimbleColor(0,0xFF,0) );
    ~GraphMeter() {delete[] myArray;}
    void drawOn( NimblePixMap& map, int x, int y ) const;
    void setLimits( float min, float max  ) {
        myMin=min;
        myMax=max;
    }
    void update( float value ) {
        Assert(myArray<=myHead);
        Assert(myHead<myEnd);
        if( ++myHead>=myEnd ) myHead=myArray;
        *myHead = value;
    }
    int width() const {return myWidth;}
    int height() const {return myHeight;}
    int currentValue() const {return *myHead;}
private:
    static const int lg2Scale = 1;
    int myWidth;
    int myHeight;
    NimblePixel myPenColor;
    //! Lower bound on data points that will be displayed without clipping
    float myMin;
    //! Upper bound on data points that will be displayed without clipping
    float myMax;
    //! Pointer to circular queue
    float* myArray;
    //! Pointer to one past last element in circular queue
    float* myEnd;
    //! Pointer to head of queue.
    float* myHead;
};

class Clickable {
public:
    enum action {
        none,
        // Returned by mouseUp when a slider was moved.
        update,
        hide
    };
    void drawOn( NimblePixMap& map, int x, int y );
    //! Returns true if mouse hits, false otherwise.
    bool mouseDown( NimblePoint p );
    void mouseMove( NimblePoint p );
    action mouseUp( NimblePoint p );
    //! True if *this, where more recently drawn, contains *this.
    bool contains( NimblePoint p ) const {
        return myPosition.x<=p.x && p.x-myPosition.x<mySize.x &&
               myPosition.y<=p.y && p.y-myPosition.y<mySize.y;
    }
private:
    //! Coordinate of upper left corner where most recently drawn.
    NimblePoint myPosition; 
    //! Dimensions of clickable area.
    NimblePoint mySize;
    virtual void doDrawOn( NimblePixMap& map ) = 0;
    virtual void doMouseDown( NimblePoint p ) = 0;
    virtual void doMouseMove( NimblePoint p ) = 0;
    virtual action doMouseUp( NimblePoint p ) = 0;
protected:
    void setClickableSize( int width, int height ) {
        mySize.x=width;
        mySize.y=height;
    }
};

//! A dialog window.
class Dialog: public Clickable, BuiltFromResourcePixMap {
public:
    int width() const {return myPixMap.width();}
    int height() const {return myPixMap.height();}
    //! Called when derived class should set values.
    virtual void setValues() {}
    using BuiltFromResourcePixMap::setResourceName;
protected:
    NimblePixMapWithOwnership myPixMap;
    Dialog( const char* resourceName );
    void buildFrom( const NimblePixMap& map ) override;

    void doDrawOn( NimblePixMap& map ) override;
    void doMouseDown( NimblePoint p ) override {};
    void doMouseMove( NimblePoint p ) override {};
    action doMouseUp( NimblePoint p ) override;
};

/** Dialog without any sliders. */
class TrivialDialog: public Dialog {
public:
    TrivialDialog( const char* resourceName ) : Dialog(resourceName) {}
};

//! Dialog with a single button
class ButtonDialog: public Dialog {
public:
    void doDrawOn( NimblePixMap& map ) override;
    void doMouseDown( NimblePoint p ) override;
    void doMouseMove( NimblePoint p ) override;
    action doMouseUp( NimblePoint p ) override;
    virtual void onSelect() = 0;
protected:
    ButtonDialog(const char* resourceName ) :
         Dialog(resourceName)
    {}
private:
    void trackMouse( NimblePoint p );
    NimbleRect myButtonRect;
    bool myButtonSelected;
    void buildFrom( const NimblePixMap& map );
};

class SliderDialog: public Dialog {
public: 
    void doDrawOn( NimblePixMap& map ) override;
    void doMouseDown( NimblePoint p ) override;
    void doMouseMove( NimblePoint p ) override;
    action doMouseUp( NimblePoint p ) override;
    virtual void changeNotice( int whichSlider, float newValue ) = 0;
    void setValue( int whichSlider, float value );
    //! Set lower and upper bounds for given slider. */
    void setLimits( int whichSlider, float min, float max  );
protected:
   SliderDialog( const char* resourceName );
private:
    static const int maxSliders=5;
    short myNumberOfValues;
    short mySelectedSlider;
    //! If true, user is dragging mySelectedSlider.
    bool myDrag;
    NimblePoint mySelectedPoint;
    short mySlotTop;
    short mySlotBottom;
    short mySlotX[maxSliders];
    float myValue[maxSliders];
    float myMinValue[maxSliders];
    float myMaxValue[maxSliders];
    void trackMouse( NimblePoint p );
    void buildFrom( const NimblePixMap& map ) override;
};

class Menu: public Clickable, public StartupListItem<Menu> {
public:
    class item {
        enum flag_type {
            enabled=1,
            checked=2
        };
    public:
        item( const char* text, const char* shortCut=nullptr ) : 
            next(nullptr),
            myText(text), 
            myShortcut(shortCut), 
            myFlags(enabled) 
        {}
        const char* text() const {return myText;}
        const char* shortcut() const {return myShortcut;}
        void setChecked( bool yes ) {
            if( yes ) myFlags|=checked;
            else myFlags&=~checked;
        }
        bool isChecked() const {return myFlags&checked;}
        void toggleChecked() {myFlags^=checked;}
        void setEnabled( bool yes ) {
            if( yes ) myFlags|=enabled;
            else myFlags&=~enabled;
        }
        bool isEnabled() const {
            return myFlags&enabled;
        }
        virtual void onSelect() = 0;
    private:
        item* next;
        const char* myText;
        const char* myShortcut;
        byte myFlags;
        friend class Menu;
    };

    class boolItem: public Menu::item {
        void onSelect() override {
            toggleChecked();
        }
    public:
        boolItem( const char* text, const char* shortCut=nullptr ) : Menu::item(text,shortCut) {}
        operator bool() const {return isChecked();}
    };


    Menu( const char* tab, Font& font );
    void append( item& i );
    int width() const {return myWidth;}
    int height() const {return myHeight;}
    bool isTabbed() const {return myState==tabbed;}
    static void constructAll();
private:
    item* root;
    item** end;
    const char* myTab;
    Font& myFont;
    //! Width of menu
    short myWidth;
    short myHeight;
    short myTabWidth;
    short myTabHeight;
    short myCheckWidth;
    short myShortcutWidth;
    static const byte noRow = -1;
    //! Currently selected row.  0 means tab itself.
    byte mySelectedRow; 
    enum state {
        //! Show only tab
        tabbed,
        //! Show full menu, do not hide if tab is clicked 
        pulled,
        //! Show full menu, hide if tab is clicked 
        armed
    };
    state myState;
    void trackMouse( NimblePoint p, bool act );
    void doDrawOn( NimblePixMap& map ) override;
    void doMouseDown( NimblePoint p ) override;
    void doMouseMove( NimblePoint p ) override;
    action doMouseUp( NimblePoint p ) override;
    void finishConstruction();
};