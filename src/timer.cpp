#include "timer.hpp"

void timers_tick(TimerSystem &ts, f32 dt_sec, void *first_arg) {
    // ticking timers
    for (u64 i = 0; auto &ge : ts.timers.things) {
        if (!ge.live) {
            goto increment;
        }

        ge.entry.t += dt_sec;

        if (ge.entry.t >= ge.entry.duration) {
            ge.entry.the_proc(first_arg, ge.entry.data);
            ts.timers.remove(GenKey{i, ge.gen});

            if (ts.clear_timers) {
                break;
            }
        }

    increment:
        ++i;
    }

    if (ts.clear_timers) {
        ts.timers.clear();
        ts.clear_timers = false;
    }
}

void timers_defer_clear(TimerSystem &ts) {
    ts.clear_timers = true;
}

GenKey timer_add(TimerSystem &ts, f32 duration, TimerProc the_proc) {
    Timer tim = {};
    tim.duration = duration;
    tim.the_proc = the_proc;
    return ts.timers.add(tim);
}

GenKey timer_add_data(TimerSystem &ts, f32 duration, TimerProc the_proc, TimerProcData data) {
    Timer tim = {};
    tim.duration = duration;
    tim.the_proc = the_proc;
    tim.data = data;
    return ts.timers.add(tim);
}
