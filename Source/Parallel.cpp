#include "Parallel.h"
#include "AssertLib.h"
#include "tbb/task_scheduler_init.h"
#include <cstdio>

static const float BusyFracSlow = 0.80; // The acceptable BusyFrac 
static const float BusyFracFast = 0.60; // The acceptable BusyFrac 
static const int LookBack = 15;         // How many frames to look back
static const int MissTolerance = 3;     // How many misses to tolerate in a LookBack period
static const int Settle = 3;            // Number of frames to ignore after changing thread count

static unsigned WasSlow;
static unsigned WasFast;

static int SettleCount = Settle;

static int ThreadCount = 1;
static tbb::task_scheduler_init TheTaskSchedulerInit(ThreadCount);

static int BitCount(unsigned n) {
    int k = 0;
    for(; n; n &= n-1) ++k;
    return k;
}

static void BumpThreadCount( int delta ) {
    Assert( 1<=ThreadCount+delta );
    Assert( ThreadCount+delta <= ThreadCount<TheTaskSchedulerInit.default_num_threads() );
    TheTaskSchedulerInit.terminate();
    ThreadCount += delta;
    std::printf("ThreadCount=%d\n",ThreadCount);
    TheTaskSchedulerInit.initialize(ThreadCount);
    WasSlow = 0;
    WasFast = 0;
    SettleCount = Settle;
}

void AdjustThreadResources( float busyFrac ) {
    if( SettleCount==0 ) {
        WasSlow = WasSlow<<1 | unsigned(busyFrac>BusyFracSlow);
        WasFast = WasFast<<1 | unsigned(busyFrac>BusyFracFast);
        int j = BitCount(WasFast & (1<<LookBack)-1);
        int k = BitCount(WasSlow & (1<<LookBack)-1);
        if( k>MissTolerance && ThreadCount<TheTaskSchedulerInit.default_num_threads() ) {
            BumpThreadCount(1);
        } else if( k==0 && j > LookBack-MissTolerance && ThreadCount>1 ) {
            BumpThreadCount(-1);
        }
    } else {
        --SettleCount;
    }
}

