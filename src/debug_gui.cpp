#ifdef _DEBUG

#include "debug_gui.hpp"

#include "imgui/imgui.h"
#include "system.hpp"
#include "level_parser.hpp"

void imgui_box_controls(Shape &b, u64 box_n) {
    string l = format("box {} controls", box_n);

    ImGui::LabelText(l.c_str(), l.c_str());

    l = format("box {}: pos", box_n);
    ImGui::DragFloat3(l.c_str(), &b.pos.x, 0.1f);
    l = format("box {}: scale", box_n);
    ImGui::DragFloat3(l.c_str(), &b.scale.x, 0.1f);
    l = format("box {}: rot", box_n);
    ImGui::DragFloat4(l.c_str(), &b.rot.x, 0.01f);
}

void imgui_controls_for_all_boxes(App &app) {
    for (u64 i = 0; i < app.es.things.size(); ++i) {
        auto &ge = app.es.things[i];

        if (!ge.live) {
            continue;
        }

        Shape &b = ge.entry.box;
        imgui_box_controls(b, i);
    }
}

void show_imgui(App &app, Renderer &renderer, f32 dt_sec) {
    const string game_name = string(GAME_NAME);
    ImGui::Begin(game_name.c_str(), NULL, ImGuiWindowFlags_NoFocusOnAppearing);

    // moving level
    // auto e = app.es.get(app.anchors[0]);
    // if (e) {
    //     imgui_box_controls(e->box, 0);
    // }

    // camera sway controls
    // {
    //     ImGui::DragFloat("ampl factor x", &app.drunk_params.amplitude_factor_x);
    //     ImGui::DragFloat("period factor x", &app.drunk_params.period_factor_x);

    //     ImGui::DragFloat("ampl factor y", &app.drunk_params.amplitude_factor_y);
    //     ImGui::DragFloat("period factor y", &app.drunk_params.period_factor_y);
    // }

    // reload levels
    {
        if (ImGui::Button("Reload levels")) {
            log("do it");
            run_command_checked("python ldtk_to_game.py --debug", ".\\docs");
            app.levels.clear();
            bool res = load_levels_from_file("assets/levels/1.lvl", app.levels);
            lassert(res);

            if (app.current_level >= app.levels.size()) {
                app.current_level = (i32)app.levels.size() - 1;
            }

            app_switch_to_level(app, app.current_level, false);
        }
    }

    // selecting level on dropdown
    {
        // ImGui::BeginListBox

        // constructing level name list from levels
        vec<char *> level_list(app.levels.size());

        vec<string> level_names(app.levels.size());

        for (u32 i = 0; i < app.levels.size(); ++i) {
            level_names[i] = format("{} - {}", i + 1, app.levels[i].name);
            level_list[i] = (char *)level_names[i].c_str();
        }

        i32 current_level = app.current_level;
        ImGui::Combo("select level", &current_level, &level_list[0], (i32)level_list.size());

        if (current_level != app.current_level) {
            app_switch_to_level(app, current_level, true);
        }

        ImGui::Text("level count: %i", app.levels.size());
        ImGui::Text("levels memory: %i bytes", sizeof(LevelNamed) * app.levels.size());
    }

    // light/mat controls
    // {
    //     ImGui::DragFloat4("mat ambient", &app.mat.Ambient.x, 0.001f);
    //     ImGui::DragFloat4("light Diffuse", &app.light.Diffuse.x, 0.001f);
    //     ImGui::DragFloat4("light specular", &app.light.Specular.x, 0.001f);
    //     ImGui::DragFloat3("light angle", &app.light.Direction.x, 0.001f);
    // }

    // camera controls
    {
        // CameraControls struct
        ImGui::DragFloat("camera - y angle", &app.cam_controls.cam_y_angle, 0.001f);
        ImGui::DragFloat("camera - x angle", &app.cam_controls.cam_x_angle, 0.001f);
        ImGui::DragFloat("camera - distance", &app.cam_controls.cam_distance);
        ImGui::DragFloat3("camera - center", &app.cam_controls.camera_center[0]);

        bool changed = ImGui::Checkbox("camera - is ortho", &app.is_cam_ortho);

        if (changed) {
            app_on_resize(app, renderer);
        }

        // Camera struct
        if (app.is_cam_ortho) {
            changed = ImGui::DragFloat("camera - ortho zoom", &app.cam_ortho_zoom, 0.001f, 0.002f, 0.4f);

            if (changed) {
                app_on_resize(app, renderer);
            }
            changed = ImGui::DragFloat("camera - ortho factor", &app.ortho_factor, 100.f, 1000.f, 0.4f);
        } else {
            changed = ImGui::DragFloat("camera - fov y", &app.cam_fovy, 0.001f);
        }

        if (changed) {
            app_on_resize(app, renderer);
        }
    }

    fps_stats_tick_and_draw(&app.fps_stats, dt_sec);

    // hierarchy test
    // imgui_controls_for_all_boxes(app);

    ImGui::End();
}

void fps_stats_tick_and_draw(FrameTimeStats *fps_stats, f32 dt_sec) {
    fps_stats->update_timer += dt_sec;
    fps_stats->dt_cache[fps_stats->timer] = dt_sec;

    ++fps_stats->timer;
    if (fps_stats->timer > frame_cache_count - 1)
        fps_stats->timer = 0;

    f32 average_dt_sec = 0.;

    for (i32 i = 0; i < frame_cache_count; ++i) {
        average_dt_sec += fps_stats->dt_cache[i];
    }

    average_dt_sec /= (f32)frame_cache_count;

    if (fps_stats->update_timer > 1.0f) {
        fps_stats->display_value = average_dt_sec;
        fps_stats->update_timer -= 1.0f;
    }

    i32 microsecs = (i32(fps_stats->display_value * 10000));
    i32 fps = (i32(1.0f / fps_stats->display_value));

    ImGui::Text("frame time: %fs", fps_stats->display_value);
    ImGui::Text("frame time: %ius", microsecs);
    ImGui::Text("FPS: %i", fps);
}

#endif
