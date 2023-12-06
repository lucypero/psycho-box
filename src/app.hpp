#pragma once

#include "lucytypes.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "gen_vec.hpp"
#include "gameplay.hpp"
#include "timer.hpp"
#include "animation.hpp"

const inline constexpr size_t frame_cache_count = 100;
const inline constexpr string_view GAME_NAME = "Psycho Box"sv;
const inline constexpr string_view BUILD_VERSION = "4"sv;

struct FrameTimeStats {
    f32 dt_cache[frame_cache_count];
    i32 timer;
    f32 update_timer;
    f32 display_value;
};

enum struct GameState { Menu, LevelSelect, InfoMenu, Game, End };

struct MenuState {
    u32 thing_selected;
};

struct DrunkParams {
    f32 amplitude_factor_x = 0.1f;
    f32 period_factor_x = 1.0f;
    f32 amplitude_factor_y = 0.05f;
    f32 period_factor_y = 2.314f;
};

const inline constexpr f32 initial_cam_angle = math::Tau * 0.0f;

struct App {
    Camera camera;
    bool should_update_cameras;
    f32 current_angle = initial_cam_angle;
    f32 ortho_factor = 800.f;

    bool is_cam_ortho = true;
    float cam_fovy = 0.490f;
    float cam_ortho_zoom = 0.4f;
    Camera text_camera;
    v3 text_cam_pos;
    CameraControlsOrbital cam_controls;
    FrameTimeStats fps_stats;
    bool draw_grid;
    DirectionalLight light;
    Material mat;

    // systems
    AnimationSystem as;
    TimerSystem ts;
    EntitySystem es;

    // imgui stuff
    string text_test;
    v2 text_offset;
    f32 text_scale;

    // all levels are stored here
    vec<LevelNamed> levels;

    // current Level state
    i32 current_level;
    Level level_c; // current state of level
    bool player_has_control = true;
    vec<Direction> level_moves;
    bool completed_game;

    vec<GenKey> anchors; // anchors for the planes

    vec<GenKey> preview_keys;

    GameState game_state = GameState::Menu;

    // menu state
    MenuState menu_state;
    DrunkParams drunk_params;
    f32 time;
};

void app_init(App &app, Ctx &ctx);
void app_tick(App &app, Ctx &ctx, f32 dt_sec);
void app_on_resize(App &app, Renderer &renderer);
void app_switch_to_level(App &app, i32 level_number, bool do_transition_anim);