#pragma once

#include "lucytypes.hpp"
#include "gen_vec.hpp"

// There's only one option now but could be more
enum struct TimerDataWhat { GenKey };

// There's only one option now but could be more
union TimerProcUnion {
    GenKey entity_key;
};

struct TimerProcData {
    TimerDataWhat what;
    TimerProcUnion data;
};

using TimerProc = void (*)(void *, TimerProcData data);

struct Timer {
    f32 t;
    f32 duration;
    TimerProcData data;
    TimerProc the_proc;
};

struct TimerSystem {
    GenVec<Timer> timers;
    bool clear_timers;
};

void timers_tick(TimerSystem &ts, f32 dt_sec, void *first_arg);
void timers_defer_clear(TimerSystem &ts);
GenKey timer_add(TimerSystem &ts, f32 duration, TimerProc the_proc);
GenKey timer_add_data(TimerSystem &ts, f32 duration, TimerProc the_proc, TimerProcData data);