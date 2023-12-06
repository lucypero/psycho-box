#pragma once

#ifdef _DEBUG

#include "lucytypes.hpp"
#include "app.hpp"
#include "renderer.hpp"

void imgui_box_controls(Shape &b, u64 box_n);
void imgui_controls_for_all_boxes(App &app);
void show_imgui(App &app, Renderer &renderer, f32 dt_sec);
void fps_stats_tick_and_draw(FrameTimeStats *fps_stats, f32 dt_sec);

#endif