#include "Parallel.h"
#include "AssertLib.h"
#include "tbb/task_scheduler_init.h"

// There is a wide variance in the total time to compute and present a frame with DirectX.
// Hence to get a more reliable estimate of BusyFrac, the code estimates
// the time looking back over a certain number of frames. 

static const unsigned TimeQueueSize = 8;    // Should be power of two for speed, though other sizes will work.
static const unsigned TimeLookback = 6;     // Number of frames to look back.  Must be less than TimeQueueSize.
static double TimeQueue[TimeQueueSize];
static unsigned TimeQueueIndex;             // Index into TimeQueueIndex
static float LastBusyFrac;

static const float BusyFracSlow = 0.80f;    // The acceptable BusyFrac 
static const float BusyFracFast = 0.50f;    // The acceptable BusyFrac 
static const int LookBack = 15;             // How many frames to look back
static const int MissTolerance = 3;         // How many misses to tolerate in a LookBack period
static const int Settle = 15;               // Number of frames to ignore after changing thread count

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

static void BumpThreadCount(int delta) {
    Assert(1<=ThreadCount+delta);
    Assert(ThreadCount+delta <= TheTaskSchedulerInit.default_num_threads());
    TheTaskSchedulerInit.terminate();
    ThreadCount += delta;
    TheTaskSchedulerInit.initialize(ThreadCount);
    WasSlow = 0;
    WasFast = 0;
    SettleCount = Settle;
}

void ThrottleWorkers(double t0, double t1) {
    // Compute BusyFrac
    TimeQueue[TimeQueueIndex] = t1;
    double oldT1 = TimeQueue[(TimeQueueIndex - TimeLookback) % TimeQueueSize];
    TimeQueueIndex = (TimeQueueIndex + 1) % TimeQueueSize;
    float busyFrac = float((t1-t0)*TimeLookback / (t1-oldT1));
    LastBusyFrac = busyFrac;

    // Act on it
    if(SettleCount==0) {
        WasSlow = WasSlow<<1 | unsigned(busyFrac>BusyFracSlow);
        WasFast = WasFast<<1 | unsigned(busyFrac<BusyFracFast);
        int j = BitCount(WasFast & (1<<LookBack)-1);
        int k = BitCount(WasSlow & (1<<LookBack)-1);
        if(k>MissTolerance && ThreadCount<TheTaskSchedulerInit.default_num_threads()) {
            // Running slower than threshold enough times that it's worth trying more threads.
            BumpThreadCount(1);
        } else if(k==0 && j >= LookBack-MissTolerance && ThreadCount>1) {
            // Running faster than necessary most of the time, and never missing a deadline, so throttle back some.
            BumpThreadCount(-1);
        }
    } else {
        --SettleCount;
    }
}

int WorkerCount() {
    return ThreadCount;
}

float BusyFrac() {
    return LastBusyFrac;
}
