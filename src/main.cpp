#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "lucytypes.hpp"
#include "app.hpp"
#include "renderer.hpp"
#include "audio.hpp"
#include "system.hpp"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR args, _In_ int) {

#ifdef _DEBUG
    // packing a release build path
    if (strcmp(args, "just_pack ") == 0) {
        pack_release_build();
        return 0;
    }
#endif

    App *app = new App{};
    Renderer *renderer = new Renderer{};
    Input *input = new Input{};
    AudioData *ad = new AudioData{};

    Ctx ctx = {};
    ctx.app = app;
    ctx.renderer = renderer;
    ctx.input = input;
    ctx.audio = ad;

    lassert_s(create_window(ctx, hInstance), "Could not create window.");
    lassert_s(init_renderer(ctx), "Could not initialize renderer");

    Audio::init(*ad);

    app_init(*app, ctx);

    bool is_running = true;

    i64 perf_freq;
    QueryPerformanceFrequency((LARGE_INTEGER *)&perf_freq);
    f64 seconds_per_count = 1.0 / (f64)perf_freq;
    i64 time_last;
    QueryPerformanceCounter((LARGE_INTEGER *)&time_last);
    renderer->seconds_per_count = seconds_per_count;

    MSG msg = {};

    // main loop
    while (is_running && !ctx.exit) {
        input->update();

        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                is_running = false;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        i64 time_now;
        QueryPerformanceCounter((LARGE_INTEGER *)&time_now);
        i64 dt = time_now - time_last;
        f32 dt_sec = (f32)((f64)dt * seconds_per_count);
        time_last = time_now;

        // draw scene
        rend::clear_frame(*renderer);
        app_tick(*app, ctx, dt_sec);
        rend::construct_frame(*renderer);
        rend::present_frame(*renderer);
    }

#ifdef _DEBUG
    rend::clean_up();
    delete renderer;
#endif

    return 0;
}