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
 OS-independent game code that is called from OS specific services on the host.
*******************************************************************************/

#include "AssertLib.h"
#include "Config.h"
#include "Airgun.h"
#include "NimbleDraw.h"
#include "Game.h"
#include "Host.h"
#include "BuiltFromResource.h"
#include "Widget.h"
#include "Geology.h"
#include "Reservoir.h"
#include "Parallel.h"
#include "Sprite.h"
#include "Wavefield.h"
#include "Seismogram.h"
#include "Utility.h"
#include <cstdlib>
#include <cmath>

static int WindowWidth, WindowHeight, PanelWidth;

#if ASSERTIONS
enum GameInterfaceCall {
    GIC_GameInitialize,
    GIC_GameResizeOrMove,
    GIC_GameUpdateDraw
};

static void CheckGameInterface( GameInterfaceCall action ) {
    static bool gameInitialized;
    static bool gameResizedOrMoved;
    switch( action ) {
        case GIC_GameInitialize:
            Assert( !gameInitialized );
            gameInitialized = true;
            break;
        case GIC_GameResizeOrMove:
            Assert( gameInitialized );
            gameResizedOrMoved = true;
            break;
        case GIC_GameUpdateDraw:
            Assert( gameInitialized && gameResizedOrMoved );
            break;
    }
}
#else
#define CheckGameInterface(new) ((void)0)
#endif /* ASSERTIONS */

class FadingBool {
    float myValue;
    bool myGoal;
public:
    FadingBool( bool value ) : myValue(value), myGoal(value) {}
    operator float() const {return myValue;}
    void operator=( bool value ) {myValue=value; myGoal=value;}
    void flipGoal() {myGoal=!myGoal;}
    bool goal() const {return myGoal;}
    void update();
};

void FadingBool::update() {
    if( myValue!=float(myGoal) ) {
        float fadeStep=1/16.f;
        if( myGoal ) {
            myValue+=fadeStep;
            if( myValue>=1.0f ) myValue=1.0f;
        } else {
            myValue-=fadeStep;
            if( myValue<0.0f ) myValue=0.0f;
        }
    }
}

static bool ShowFrameRate=false;
static FadingBool ShowGeology=true;
static FadingBool ShowSeismic=true;
static ColorFunc TheColorFunc;

static GeologyParameters TheGeologyParameters;
static AirgunParameters TheAirgunParameters;

static float EstimateFrameRate() {
    static double t0;
    static int count;
    static double estimate;
    ++count;
    double t1 = HostClockTime();
    if( t1-t0>=1.0 ) {
        estimate = count/(t1-t0);
        t0 = t1;
        count = 0;
    } 
    return estimate;
}

static Dialog* VisibleDialog = NULL;

//! Maximum allowed depth of water.
/** Value needs to be above about .7 to avoid problem in the geology generator. */
static const float DeepWaterFractionMax = .666666f;
static const float CurvatureMax = 0.5f;
static const float CurvatureMin = 0.1f;

class GeologySliderDialog: public SliderDialog {
public:
    GeologySliderDialog() : SliderDialog("GeologySliders") {
        setLimits(0,1-DeepWaterFractionMax,1);
        setLimits(1,0,1);
        setLimits(3,CurvatureMin,CurvatureMax);
        setLimits(4,1,4);
    }
    void setValues() {
        setValue(0,1-TheGeologyParameters.oceanDepth);
        setValue(1,1-TheGeologyParameters.sandstoneDepth);
        setValue(2,TheGeologyParameters.dip);
        setValue(3,TheGeologyParameters.curvature);
        setValue(4,TheGeologyParameters.nBump);
    }
private:
    /*override*/void changeNotice( int whichSlider, float newValue ) {
        switch( whichSlider ) {
            case 0:
                TheGeologyParameters.oceanDepth = 1-newValue;
                Assert( TheGeologyParameters.oceanDepth<=DeepWaterFractionMax );
#if 0
                if( TheGeologyParameters.oceanDepth + TheGeologyParameters.curvature > 0.9f ) {
                    TheGeologyParameters.curvature = 0.9f-TheGeologyParameters.oceanDepth;
                    setValue(2,TheGeologyParameters.curvature);
                }
#endif
                break;
            case 1:
                TheGeologyParameters.sandstoneDepth = 1-newValue;
                break;
            case 2:
                TheGeologyParameters.dip = newValue;
                break;
            case 3:
                Assert( 0<=newValue && newValue<=1.0f );
                TheGeologyParameters.curvature = newValue;
#if 0
                if( TheGeologyParameters.oceanDepth + TheGeologyParameters.curvature > 0.9f ) {
                    TheGeologyParameters.oceanDepth = 0.9f-TheGeologyParameters.curvature;
                    setValue(0,TheGeologyParameters.oceanDepth);
                }
#endif
                break;
            case 4:
                TheGeologyParameters.nBump = Round(newValue);
                setValue(4,TheGeologyParameters.nBump);
                break;
        }
    }
};

static GeologySliderDialog TheGeologyDialog;

class ShotSliderDialog: public SliderDialog {
public:
    ShotSliderDialog() : SliderDialog("ShotSliders") {
        setLimits(0,0,APK_N_SIGNATURE-1);
        float tmp = logf(2);
        setLimits(1,-tmp,tmp);
        setLimits(2,-tmp,tmp);
    }
    void setValues() {
        setValue(0,3-TheAirgunParameters.pulseKind);
        setValue(1,logf(TheAirgunParameters.frequency));
        setValue(2,logf(TheAirgunParameters.amplitude));
    }
private:
    /*override*/void changeNotice( int whichSlider, float newValue ) {
        switch( whichSlider ) {
            case 0: {
                int rounded = Round(newValue);
                setValue(0,rounded);
                TheAirgunParameters.pulseKind = AirgunPulseKind(3-rounded);
                Assert(0<=TheAirgunParameters.pulseKind && TheAirgunParameters.pulseKind<APK_N_SIGNATURE );
                break;
            }
            case 1:
                TheAirgunParameters.frequency = expf(newValue);
                break;
            case 2:
                TheAirgunParameters.amplitude = expf(newValue);
                break;
        }
        AirgunInitialize( TheAirgunParameters );
    }
};

static ShotSliderDialog TheShotDialog;

class ColorSliderDialog: public SliderDialog {
public:
    ColorSliderDialog() : SliderDialog("ColorSliders") {
        setLimits(0,0,2);
    }
    void setValues() {
        setValue(0,TheColorFunc);
    }
private:
    /*override*/void changeNotice( int whichSlider, float newValue ) {
        switch( whichSlider ) {
            case 0: {
                int rounded = Round(newValue);
                setValue(0,rounded);
                TheColorFunc = ColorFunc(rounded);
                break;
            }
        }
    }
};

static ColorSliderDialog TheColorDialog;

class SpeedDialog: public SliderDialog {
public:
    SpeedDialog() : SliderDialog("SpeedSliders") {
        setLimits(0,1,5);
        setLimits(1,0,2);
        oldIntervalLimit=1;
    }
    void setValues() {
        setValue(0,WavefieldGetPumpFactor());
        setValue(1,oldIntervalLimit);
    }
private:
    int oldIntervalLimit;
public:
    /*override*/void changeNotice( int whichSlider, float newValue ) {
        switch( whichSlider ) {
            case 0: {
                int rounded = Round(newValue);
                setValue(whichSlider,rounded);
                WavefieldSetPumpFactor(rounded);
                break;
            }
            case 1: {
                int rounded = Round(newValue);
                setValue(whichSlider,rounded);
                if( rounded!=oldIntervalLimit ) {
                    oldIntervalLimit = rounded; 
                    HostSetFrameIntervalRate( 2-rounded );
                }
                break;
            }
        }
    }
};

static SpeedDialog TheSpeedDialog;

class MessageDialog: public ButtonDialog {
    void onSelect() {VisibleDialog=NULL;}
public:
    MessageDialog( const char* resourceName ) : ButtonDialog(resourceName) {}
};

static MessageDialog TheAboutTheAuthorDialog("AboutTheAuthor");
static MessageDialog TheKeyboardHelpDialog("Keyboard");

static Font TheFont("FontSans16");

class DialogItem: public Menu::item {
    Dialog& myDialog;
    /*override*/ void onSelect() {
        myDialog.setValues();
        VisibleDialog=&myDialog;
    }
public:
    DialogItem( const char* text, Dialog& d ) : Menu::item(text), myDialog(d) {}
};

class BeginGameItem: public Menu::item {
    /*override*/ void onSelect();
public:
    BeginGameItem() : Menu::item("Begin Game") {}
};

class ExploreNewAreaItem: public Menu::item {
    /*override*/ void onSelect();
public:
    ExploreNewAreaItem() : Menu::item("Explore New Area","n") {}
};

class EndGameItem: public Menu::item {
    /*override*/ void onSelect();
public:
    EndGameItem() : Menu::item("End Game") {}
};

class ExitItem: public Menu::item {
    /*override*/ void onSelect() {HostExit();}
public:
    ExitItem() : Menu::item("Exit","Esc") {}
};

static BeginGameItem TheBeginGameItem;
static ExploreNewAreaItem TheExploreNewAreaItem;
static EndGameItem TheEndGameItem;
static Menu::boolItem IsPaused("Pause","p");
static ExitItem TheExitItem;

class FileMenu: public Menu {
public:
    // FIXME - put separator before and after pause.
    FileMenu() : Menu("File",TheFont) {
        TheEndGameItem.setEnabled(false);
        append(TheBeginGameItem);
        append(TheExploreNewAreaItem);
        append(TheEndGameItem);
        append(IsPaused);
        append(TheExitItem);
    }
};

static FileMenu TheFileMenu;

static Menu::boolItem IsAutoGainOn("Auto Gain","a");
static DialogItem TheModelGeologyItem("Geology...",TheGeologyDialog);
static DialogItem TheModelShotItem("Shot...",TheShotDialog);

class ModelMenu: public Menu {
public:
    ModelMenu() : Menu("Model",TheFont) {
        append(IsAutoGainOn);
        append(TheModelGeologyItem);
        append(TheModelShotItem);
    }
};

static ModelMenu TheModelMenu;

class GeologyItem: public Menu::item {
    /*override*/ void onSelect();
public:
    GeologyItem() : Menu::item("Geology","g") {}
} TheGeologyItem;

static Menu::boolItem ShowReservoir("Reservoir","r");

class SeismicItem: public Menu::item {
    /*override*/ void onSelect();
public:
    SeismicItem() : Menu::item("Seismic","s") {}
} TheSeismicItem;

static DialogItem TheColorItem("Color...",TheColorDialog);
static DialogItem TheSpeedItem("Speed...",TheSpeedDialog);

class ViewMenu: public Menu {
public:
    ViewMenu() : Menu("View",TheFont) {
        append(TheGeologyItem);
        append(ShowReservoir);
        append(TheSeismicItem);
        append(TheColorItem);
        append(TheSpeedItem);
    }
};

static ViewMenu TheViewMenu;

class AboutItem: public Menu::item {
    /*override*/ void onSelect() {VisibleDialog = &TheAboutTheAuthorDialog;}
public:
    AboutItem() : Menu::item("About Seismic Duck") {}
} TheAboutItem;

class KeyboardItem: public Menu::item {
    /*override*/ void onSelect() {VisibleDialog = &TheKeyboardHelpDialog;}
public:
    KeyboardItem() : Menu::item("Keys") {}
} TheKeyboardItem;

class HelpMenu: public Menu {
public:
    HelpMenu() : Menu("Help",TheFont) {
        append(TheAboutItem);
        append(TheKeyboardItem);
    }
};

static HelpMenu TheHelpMenu;

static TrivialDialog TheBankruptDialog("Bankrupt");

static float PhasePrice[N_Phase];
static const float TimePrice = 0.005f;
static float DrillPrice;

static NimbleRect WavefieldRect;

//! Horizontal position and velocity of a Duck or Rig
class SurfaceX {
    /** Units = pixels horizontally from origin. 
        Is a float because position can be fractional */
    float position;
    /** Units = pixels per second */
    float velocity;
    //! Maximum allowed speed for object.
    /** Units = wavefield widths per second. */
    float speedLimit;
public:
    SurfaceX( float speedLimit_ ) : speedLimit(speedLimit_) {}
    operator int() const {
        return Round(position);
    }
    void set( float x ) {
        velocity=0;
        position=x;
    }
    /** dir specifies the requested direction of movement.
        It should be -1, 0, or 1. */
    void update( int dir, float dt );
};

void SurfaceX::update( int dir, float dt ) {
    Assert(-1<=dir && dir<=1);
    if( dir==0 ) {
        velocity=0;
    } else {
        // Units is in pixels/second
        float maxX = WavefieldRect.width()-1;
        const float maxSpeed = maxX*speedLimit;
        const float minSpeed = maxSpeed*(1.0f/12);
        float acc = 0.25f*(maxSpeed-minSpeed)*dir;
        float v = velocity+acc*dt;
        if( fabs(v)<minSpeed )
            v = v<0 ? -minSpeed : minSpeed;
        else if( fabs(v)>maxSpeed )
            v = v<0 ? -maxSpeed : maxSpeed;
        velocity = v;
        position = Clip( 0.f, maxX, position+v*dt );
    } 
}

static WheelMeter CashMeter("CashMeter");
static WheelMeter LevelMeter("LevelMeter");
static BarMeter WaterMeter("WaterMeter");
static BarMeter OilMeter("OilMeter");
static BarMeter GasMeter("GasMeter");
static RubberImage PanelBackground("Panel");
static DigitalMeter FrameRateMeter(6,1);
static BarMeter BusyMeter("BusyMeter",/*isVertical=*/false);

static MessageDialog TheLevelContinueDialog("LevelContinueDialog");
static MessageDialog WarnBreakDrillDialog("WarnBreakDrillDialog");
static MessageDialog WarnAwayFromCultureDialog("WarnAwayFromCultureDialog");
static const int ClickableSetMaxSize = 4;
static Clickable* ClickableSet[ClickableSetMaxSize]; 
static Clickable** ClickableSetEnd = ClickableSet;

void DrawClickable( Clickable& clickable, NimblePixMap& map, int x, int y ) {
    Assert( ClickableSetEnd<&ClickableSet[ClickableSetMaxSize]);
    *ClickableSetEnd++ = &clickable;
    clickable.drawOn(map,x,y);
}

static SurfaceX RigX(0.25f);
static int DrillY;

class ScoreStateMachine {
    enum gameStateType {
        //! Not playing game
        gsTraining,
        //! Playing game and keeping score
        gsPlaying,
        //! Playing game and acquiring points
        gsBonus,
        //! Waiting for user to click 
        gsWait,
        //! End of game
        gsOver
    };
    gameStateType state;
    double bonusStartTime;
    bool myHasCulture;
public:
    //! True if in training mode.
    /** Allows user to change parameters, peek at geology, seismic, reservoir, etc. */
    bool isTraining() const {
        return state==gsTraining;
    }
    //! True if score is being kept.
    /** Normally the value is same as IsPlayingGame.  However, when debugging
        it is sometimes useful to change the definition to "return true". */
    bool isDisplayingScore() const {
        return state!=gsTraining;
    }
    bool isUpdatingScore() const {
        return state==gsPlaying||state==gsBonus;
    }
    //! True if player is allowed to start a new hole
    bool isNewDrillingAllowed() const {
        return state==gsTraining||state==gsPlaying;
    }
    //! True if player is allowed to peek underground
    bool isPeekAllowed() const {
        return state==gsTraining||state==gsBonus||state==gsOver;
    }
    //! True if there is culture
    bool hasCulture() const {return myHasCulture;}

    //! Get geology parameters for this level.
    /** Also sets myHasCulture. */
    GeologyParameters geologyParametersOfLevel();

    void startNewArea();
    void startGame();
    void finishGame();
    void update();
};

static ScoreStateMachine ScoreState;

static void MoveDrillVertically( int direction ) {
    Assert(DrillY>=0);
    if( direction>0 && DrillY==0 && ScoreState.isNewDrillingAllowed() ) {
        RigX.set(ReservoirStartHole(RigX));
    }
    int additionalDepth = ReservoirUpdateHole(DrillY,direction);
    if( ScoreState.isUpdatingScore() )
        CashMeter -= DrillPrice*additionalDepth;
}

static Sprite LandRig("LandRig");
static Sprite FloatingRig("FloatingRig");
static Sprite* OilRig = &LandRig;
static int OilRigVerticalOffset;
static AnimatedSprite DrillBit("DrillBit",9);

static SurfaceX DuckX(1.0f);
static Sprite DuckLeft("Mallard",false);
static Sprite DuckRight("Mallard",true);
static bool DuckGoingLeft;

static Sprite Culture("Culture");
static int CultureBeginX;
static int CultureEndX;

#define WRITING_DOCUMENTATION 0

#if WRITING_DOCUMENTATION
static NimblePixMap TheMap;
#include <new>
#endif /* WRITING_DOCUMENTATION */

static void UpdateDuckAndRig() {
    static double T0;
    double t1 = HostClockTime();
    float dt = t1-T0;
    T0 = t1;
    if( dt==t1 ) 
        return;

    int d = HostIsKeyDown( 'x' )-HostIsKeyDown( 'z' );
    DuckX.update( d, dt );
    if( d!=0 ) 
        DuckGoingLeft = d<0;

    d = HostIsKeyDown( HOST_KEY_RIGHT ) - HostIsKeyDown( HOST_KEY_LEFT ); 
    if( d && DrillY>0 )
        VisibleDialog = &WarnBreakDrillDialog;
    else
        RigX.update(d,dt);
    if( HostIsKeyDown( HOST_KEY_DOWN ) )
        MoveDrillVertically(1);
    if( HostIsKeyDown( HOST_KEY_UP ) )
        MoveDrillVertically(-1);
}

class ReservoirFunctor {
    const NimbleRequest request;
public:
    ReservoirFunctor( NimbleRequest request_ ) : request(request_) {}
    void operator()() const {
        if( request & NimbleUpdate ) {
            float amount[N_Phase];
            ReservoirUpdate( amount );
            Assert( amount[0]<=1E10 );
            const float fluidScale=1.0f;
            GasMeter.setValue( amount[GAS]*fluidScale );
            OilMeter.setValue( amount[OIL]*fluidScale );
            WaterMeter.setValue( amount[WATER]*fluidScale );
            Assert(fabsf(CashMeter.value())<=1E6);
            if( ScoreState.isUpdatingScore() ) 
                for( int k=0; k<N_Phase; ++k )
                    CashMeter+=amount[k]*PhasePrice[k];
            Assert(fabsf(CashMeter.value())<=1E6);
        }
    }
};

void GameUpdateDraw( NimblePixMap& map, NimbleRequest request ) {
    CheckGameInterface(GIC_GameUpdateDraw); 

#if WRITING_DOCUMENTATION
    new(&TheMap) NimblePixMap( map );
#endif /* WRITING_DOCUMENTATION */
    WavefieldRect = NimbleRect(PanelWidth,map.height()/2,map.width(),map.height());
    NimblePixMap subsurface(map,WavefieldRect); 
    NimblePixMap seismogramClip( map,NimbleRect(PanelWidth,0,map.width(),map.height()/2) );

    if( request & NimbleUpdate ) {
        ShowGeology.update(); 
        ShowSeismic.update(); 
    }
    const NimbleRequest pausedRequest = IsPaused ? request-NimbleUpdate : request;
	// Update the seismogram but do not draw it, using the current wavefield state.  
    SeismogramUpdateDraw( seismogramClip, pausedRequest&NimbleUpdate, TheColorFunc, IsAutoGainOn );

	// Do the computationally intense tasks in parallel
	auto wf = [=]{WavefieldUpdateDraw(subsurface, pausedRequest, ShowGeology, ShowSeismic, TheColorFunc);  };
	// Functor for drawing the seismogram.
	auto sf = [=]{SeismogramUpdateDraw(seismogramClip, pausedRequest&NimbleDraw, TheColorFunc, IsAutoGainOn); };
    ReservoirFunctor rf( pausedRequest );
#if USE_TBB
    tbb::parallel_invoke( wf, sf, rf );
#elif USE_CILK
    cilk_spawn wf();
    cilk_spawn sf();
    rf();
    cilk_sync;
#else
    wf();
    sf();
    rf();
#endif
    if( pausedRequest & NimbleUpdate ) {
        DrillBit.update();
        UpdateDuckAndRig();
    }
    if( request & NimbleDraw ) {
        ClickableSetEnd = ClickableSet;
#if ASSERTIONS
        const int wavefieldWidth = map.width()-PanelWidth;
        Assert(wavefieldWidth%4==0);
#endif /* ASSERTIONS */
        ReservoirDrawHoles( subsurface );
        if( DrillY>0 )
            DrillBit.drawOn( subsurface, RigX-DrillBit.width()/2, DrillY-5 );
        if( ShowReservoir )
            ReservoirDraw( subsurface );
        NimblePixMap spriteClip( map, NimbleRect( PanelWidth, 0, map.width(), map.height() ));\
        NimblePixel greenPixel = NimbleColor( 0, NimbleColor::full, 0 ).pixel();
        NimblePixel redPixel = NimbleColor( NimbleColor::full, 0, 0 ).pixel();
        const int lineHeight = map.height()/2-1; 
        if( CultureBeginX<CultureEndX ) {
            spriteClip.draw( NimbleRect( 0, lineHeight, CultureBeginX, lineHeight+1 ), greenPixel ); 
            spriteClip.draw( NimbleRect( CultureBeginX, lineHeight-1, CultureEndX, lineHeight+2 ), redPixel ); 
            spriteClip.draw( NimbleRect( CultureEndX, lineHeight, spriteClip.width(), lineHeight+1 ), greenPixel ); 
            Culture.drawOn( spriteClip, CultureBeginX+(CultureEndX-CultureBeginX)/2-Culture.width()/2, map.height()/2-Culture.height() );
        } else {
            spriteClip.draw( NimbleRect( 0, lineHeight, spriteClip.width(), lineHeight+1 ), greenPixel ); 
        }
        OilRig->drawOn( spriteClip, RigX-OilRig->width()/2, OilRigVerticalOffset ); 
        if( DuckGoingLeft )
            DuckLeft.drawOn( spriteClip, DuckX-50, map.height()/2-DuckLeft.height()+12 );
        else
            DuckRight.drawOn( spriteClip, DuckX-(DuckRight.width()-50), map.height()/2-DuckLeft.height()+12 );
        NimblePixMap panelClip( map, NimbleRect( 0, 0, PanelWidth, map.height() ));

        PanelBackground.drawOn( panelClip );

        int cashMeterY = map.height()- 50 -CashMeter.height();
        int levelMeterY = cashMeterY - 10 - LevelMeter.height();
        // The Min is there so that if there is room, the green line artistically lines up 
        // with the top of the fluid meters.
        int fluidMeterY = Min(levelMeterY - 10 - WaterMeter.height(), map.height()/2 );
        if( ScoreState.isDisplayingScore() ) {
            LevelMeter.drawOn( map, PanelWidth/2-LevelMeter.width()/2, levelMeterY );
            CashMeter.drawOn( map, PanelWidth/2-CashMeter.width()/2, cashMeterY );
        }
        int meterMarginX = (PanelWidth-WaterMeter.width()-OilMeter.width()-GasMeter.width())/4;
        WaterMeter.drawOn( map, meterMarginX, fluidMeterY );
        OilMeter.drawOn( map, PanelWidth/2-OilMeter.width()/2, fluidMeterY );
        GasMeter.drawOn( map, PanelWidth-meterMarginX-GasMeter.width(), fluidMeterY );
        if( VisibleDialog ) {
            if( VisibleDialog==&TheAboutTheAuthorDialog || VisibleDialog==&TheLevelContinueDialog )
                // Center it on screen
                VisibleDialog->drawOn( map, map.width()/2-VisibleDialog->width()/2, map.height()/2-VisibleDialog->height()/2 );
            else if( VisibleDialog==&TheKeyboardHelpDialog )
                // Put in upper right corner
                VisibleDialog->drawOn( map, map.width()*.95f-VisibleDialog->width(), map.height()*.05f );
            else
                // Put it in upper left portion of seismogram
                VisibleDialog->drawOn( map, PanelWidth+24, 24 );
        }
        int tabTop1 = 50;
        int tabTop2 = tabTop1+2*TheFont.height();
        int tabLeft1 = PanelWidth/8;
        int tabLeft2 = PanelWidth*5/8;
        int airGunTop = tabTop2+2*TheFont.height();;
        AirgunMeter.drawOn( map, PanelWidth/2-AirgunMeter.width()/2, airGunTop );
        if( ShowFrameRate ) {
            FrameRateMeter.setValue( EstimateFrameRate() );
            FrameRateMeter.drawOn( map, PanelWidth/2-FrameRateMeter.width()/2, fluidMeterY-FrameRateMeter.height()-15 ); 
            BusyMeter.setValue( HostBusyFrac() );
            BusyMeter.drawOn( map, PanelWidth/2-BusyMeter.width()/2, fluidMeterY-FrameRateMeter.height()-FrameRateMeter.height()-25 );
        }
        DrawClickable( TheFileMenu, map, tabLeft1, tabTop1 );
        DrawClickable( TheHelpMenu, map, tabLeft2, tabTop1 );
        // The "isTabbed" tests below do some ad-hoc occlusion testing.
        if( TheFileMenu.isTabbed() ) 
            DrawClickable( TheModelMenu, map, tabLeft1, tabTop2 );
        if( TheFileMenu.isTabbed() && TheHelpMenu.isTabbed() )
            DrawClickable( TheViewMenu, map, tabLeft2, tabTop2 );
    }
    ScoreState.update();
}

bool GameInitialize() {
    CheckGameInterface(GIC_GameInitialize);
    BuiltFromResourcePixMap::loadAll();
    AirgunInitialize( TheAirgunParameters );
    CashMeter.setValue(100);
    TheSpeedDialog.setValues();
    Menu::constructAll();
    TheGeologyItem.setChecked(ShowGeology.goal());
    TheSeismicItem.setChecked(ShowSeismic.goal());
#if 0
    srand(2);
#else
    srand( unsigned( fmod( HostClockTime()*1E3, 4*double(1<<30))));
#endif

    return true;
}

static void CreateNewArea( bool recycle=false ) {
    if( VisibleDialog==&TheBankruptDialog )
        VisibleDialog = NULL;
    Assert( (WindowWidth-PanelWidth)%4==0 );
    ReservoirStats s;
    if( recycle ) {
        TheGeology.generate( TheGeologyParameters, WindowWidth-PanelWidth+2*HIDDEN_BORDER_SIZE, WindowHeight/2+HIDDEN_BORDER_SIZE );
    } else {
        GeologyParameters gp = ScoreState.isTraining() ? TheGeologyParameters : ScoreState.geologyParametersOfLevel();
        float best=-1.0f;
        // Try three sample geologies, and choose one with biggest volume
        for( int trial=0; trial<3; ++trial ) {
            gp.random.randomize();
            Geology g;
            g.generate( gp, WindowWidth-PanelWidth+2*HIDDEN_BORDER_SIZE, WindowHeight/2+HIDDEN_BORDER_SIZE );
            ReservoirInitialize(s,g);
            if( s.volume[GAS]+s.volume[OIL]>best ) {
                TheGeology = g;
                if( ScoreState.isTraining() ) {
                    // Save random part, so it can be replayed if user changes parameters.
                    TheGeologyParameters.random = gp.random;
                }
            }
        }
    }
    ReservoirInitialize(s,TheGeology);
    float totalWorth = 400.0f;
    float oilToGasPriceRatio = 4.0f;
    if( s.volume[GAS]==0 ) 
        // Keep equations from blowing up.  User is out of luck anyway.
        s.volume[GAS]=1;
    PhasePrice[GAS] = totalWorth/(s.volume[GAS]+s.volume[OIL]*oilToGasPriceRatio);
    PhasePrice[OIL] = totalWorth/(s.volume[OIL]+s.volume[GAS]/oilToGasPriceRatio);
    PhasePrice[WATER] = 0;

    WavefieldInitialize( TheGeology );
    int fieldWidth = WindowWidth-PanelWidth;
    SeismogramReset( fieldWidth, WindowHeight/2 );
    DuckX.set(0.5f*fieldWidth);
    RigX.set( 0.6f*fieldWidth);
    DrillY = 0;
    // Two holes to the bottom result in bankruptcy.
    DrillPrice = 100.0f/(WindowHeight-TheGeology.oceanFloor());

    if( TheGeology.oceanFloor()>=24 ) {
        OilRig = &FloatingRig;
        OilRigVerticalOffset = WindowHeight/2-70;
    } else {
        OilRig = &LandRig;
        OilRigVerticalOffset = WindowHeight/2+TheGeology.oceanFloor()-LandRig.height();
    }
    if( ScoreState.hasCulture() ) {
        int x = float(fieldWidth)*std::rand()/RAND_MAX;
        CultureBeginX = x-0.25f*fieldWidth;
        CultureEndX = x+0.25f*fieldWidth;
    } else {
        CultureBeginX = CultureEndX = -1;
    }

}

void GameResizeOrMove( NimblePixMap& map ) {
    CheckGameInterface( GIC_GameResizeOrMove );
    WindowWidth = map.width();
    WindowHeight = map.height();
    // Set PanelWidth so that remaining width is mulitple of 4
    PanelWidth = PANEL_MIN_WIDTH+WindowWidth%4;
    CreateNewArea();
}

static void EnableGeologyReservoirSeismicKeys( bool value ) {
    TheGeologyItem.setEnabled(value);
    ShowReservoir.setEnabled(value);
    TheSeismicItem.setEnabled(value);
}

static void ToggleShowGeology( bool quickOff=false ) {
    if( quickOff ) {
        ShowGeology=false;
    } else {
        if( !ScoreState.isPeekAllowed() ) return;
        ShowGeology.flipGoal();
    }
    TheGeologyItem.setChecked(ShowGeology.goal());
}

static void ToggleShowReservoir( bool quickOff=false ) {
    if( quickOff ) {
        ShowReservoir.setChecked(false); 
    } else {
        if( !ScoreState.isPeekAllowed() ) return;
        ShowReservoir.toggleChecked();
    }
}

static void ToggleShowSeismic( bool quickOff=false ) {
    if( quickOff ) {
        ShowSeismic=false;
    } else {
        if( !ScoreState.isPeekAllowed() ) return;
        ShowSeismic.flipGoal();
    }
    TheSeismicItem.setChecked( ShowSeismic.goal() );
}

void GeologyItem::onSelect() {ToggleShowGeology();}
void SeismicItem::onSelect() {ToggleShowSeismic();}

void BeginGameItem::onSelect() {ScoreState.startGame();}
void ExploreNewAreaItem::onSelect() {CreateNewArea();}
void EndGameItem::onSelect() {ScoreState.finishGame();}

const char* GameTitle() {
    return "Seismic Duck 2.0.5"
#if ASSERTIONS
           " ASSERTIONS"
#endif
    ;
}

void GameKeyDown( int key ) {
    Assert( !('A'<=key && key<='Z') );  // Alphabetic key should be lower case.
    switch(key) {
        case HOST_KEY_ESCAPE:
            HostExit();
            return;
        case HOST_KEY_RETURN:
            VisibleDialog = NULL;
            break;
        case ' ':
            if( CultureBeginX<=DuckX && DuckX<CultureEndX ) 
                VisibleDialog = &WarnAwayFromCultureDialog;
            else
                // Fire airgun at Duck's feet, becaus firing airgun at surface generates wave that
                // mostly cancels itself.
                AirgunFire( DuckX, 8 );
            break;
        case 'a':
            IsAutoGainOn.toggleChecked();
            break;
        case 'f':
            ShowFrameRate = !ShowFrameRate;
            break;
        case 'n':
            if( ScoreState.isTraining() )
                CreateNewArea();
            else
                ScoreState.startNewArea();
            break;
        case 'p':
            IsPaused.toggleChecked();
            break;
        case 'g':
            ToggleShowGeology();
            break;
        case 'r':
            ToggleShowReservoir();
            break;
        case 's':
            ToggleShowSeismic();
            break;
        case 'u':                           // Unlimit frame rate and show it
            TheSpeedDialog.changeNotice(0,5);
            TheSpeedDialog.changeNotice(1,2);
            ShowFrameRate = true;
            break;
#if WRITING_DOCUMENTATION
        case '0':
            IsPaused=false;
            WavefieldInitialize( TheGeology );
            SeismogramReset( WindowWidth-PanelWidth, WindowHeight/2 );
            AirgunFire( WavefieldRect.width()/2, 8 );
            for( int i=0; i<70; ++i )
                GameUpdateDraw(TheMap,NimbleUpdate);
            DuckX=0;
            RigX=0;
            IsPaused=true;
            break;
#endif /* WRITING_DOCUMENTATION */

#if ASSERTIONS
        case '3': {
            CashMeter += 30;
            break;
        }
        case '4': {
            CashMeter -= 20;
            break;
        }
        case '5': {
            ShowSeismic = true;
            ShowGeology = true;
            ShowReservoir.setChecked(true);
            break;
        }
#endif
    }
}

void GameMouseButtonDown( const NimblePoint& point, int k ) {
    switch(k) {
        case 0:
            if( VisibleDialog ) 
                if( VisibleDialog->mouseDown(point) )
                    return;
            for( Clickable** c=ClickableSetEnd; --c>=ClickableSet;  ) 
                if( (*c)->mouseDown(point) )
                    return;
            if( ScoreState.isTraining() && WavefieldRect.contains(point) ) {
                AirgunFire(point.x-WavefieldRect.left,point.y-WavefieldRect.top);
            }
            break;
    }
}

void GameMouseButtonUp( const NimblePoint& point, int k ) {
    if( VisibleDialog ) {
        switch( VisibleDialog->mouseUp(point) ) {
            case Dialog::update:
                if( VisibleDialog==&TheGeologyDialog )
                    CreateNewArea(/*recycle=*/true);
                else if( VisibleDialog==&TheShotDialog )
#if 0
                    // Show example of shot
                    GameKeyDown(' ');
#else
                    (void)0;
#endif
                break;
            case Dialog::hide:
                VisibleDialog = NULL;
                break;
            default:
                break;
        }
    }
    for( Clickable** c=ClickableSet; c!=ClickableSetEnd; ++c ) 
        (*c)->mouseUp(point);
}

void GameMouseMove( const NimblePoint& point ) {
    if( VisibleDialog ) 
        VisibleDialog->mouseMove(point);
    for( Clickable** c=ClickableSet; c!=ClickableSetEnd; ++c )
        (*c)->mouseMove(point);
}

GeologyParameters ScoreStateMachine::geologyParametersOfLevel() {
    Assert(state!=gsTraining);
    GeologyParameters result;
    int n = Round(LevelMeter.value());
    result.curvature = 0.25f;
    result.dip = 0;
    result.nBump = 1;
    result.oceanDepth = 0;
    result.sandstoneDepth = n%2==0 ? 0.75 : 0.25;
    switch( n ) {
        case 0:
        case 1:
        case 2:
            break;
        case 3:
            result.nBump = 2;
            break;
        case 4:
            result.oceanDepth = 0.25f;
            break;
        case 5:
            result.nBump = 2;
            result.oceanDepth = 0.25f;
            break;
        case 6:
            result.nBump = 1;
            myHasCulture = true;
            break;
        case 7:
            result.nBump = 1;
            result.dip = 0.5f;
            myHasCulture = true;
            break;
        case 8:
            result.nBump = 2;
            result.dip = 0.5f;
            result.oceanDepth = 0.5f;
            break;
        case 9:
            result.nBump = 3;
            result.dip = 0.5f;
            result.oceanDepth = 0.33f;
            result.curvature = 0.25f;
            break;
        default:
            result.nBump = 4;
            result.dip = 0.5f;
            result.oceanDepth = 0.33f;
            result.curvature = powf(0.75,n-8);
            break;
    }
    return result;
}

void ScoreStateMachine::startGame() {
    LevelMeter.setValue(1);
    EnableGeologyReservoirSeismicKeys(false);
    TheBeginGameItem.setEnabled(false); 
    TheEndGameItem.setEnabled(true);
    TheModelGeologyItem.setEnabled(false);
    CashMeter.setValue(100);
    startNewArea();
}

void ScoreStateMachine::finishGame() {
    state = gsTraining;
    TheBeginGameItem.setEnabled(true); 
    TheEndGameItem.setEnabled(false);
    TheModelGeologyItem.setEnabled(true);
    EnableGeologyReservoirSeismicKeys(true);
    if( VisibleDialog==&TheBankruptDialog || VisibleDialog==&TheLevelContinueDialog )
        VisibleDialog = NULL;
}

void ScoreStateMachine::startNewArea() {
    // Change state first, to disable peeking.
    state = gsPlaying;
    myHasCulture = false;
    if( ShowGeology ) ToggleShowGeology(true);
    if( ShowReservoir ) ToggleShowReservoir(true);
    if( ShowSeismic ) ToggleShowSeismic(true);
    CreateNewArea();
}

void ScoreStateMachine::update() {
    switch( state ) {
        case gsTraining:
            break;
        case gsPlaying: {
            if( IsPaused ) return;
            CashMeter -= TimePrice;
            bool revealSubsurface = false;
            if( CashMeter.value()>=200 ) {
                state = gsBonus;
                bonusStartTime = HostClockTime();
                revealSubsurface = true;
            } else if( CashMeter.value()<=0 ) {
                CashMeter.setValue(0);
                VisibleDialog = &TheBankruptDialog;
                state = gsOver;
                TheBeginGameItem.setEnabled(true); 
                TheEndGameItem.setEnabled(false);
                revealSubsurface = true;
            }
            if( revealSubsurface ) {
                if( !ShowGeology ) ToggleShowGeology();
                if( !ShowReservoir ) ToggleShowReservoir();
            }
            break;
        }
        case gsBonus:
            if( HostClockTime()>bonusStartTime+0.5 ) {
                state = gsWait;
                VisibleDialog = &TheLevelContinueDialog;
            }
            break;
        case gsWait:
            if( VisibleDialog!=&TheLevelContinueDialog ) {
                LevelMeter+=1;
                CashMeter.setValue(100);
                startNewArea();
            }
            break;
        case gsOver:
            if( VisibleDialog!=&TheBankruptDialog ) {
                finishGame();
            }
            break;
        default:
            Assert(false);
            break;
    }
}