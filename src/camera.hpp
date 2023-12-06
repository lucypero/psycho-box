#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;

#include "input.hpp"
#include "lucytypes.hpp"

struct Camera {
    // Camera coordinate system with coordinates relative to world space.
    v3 position;
    v3 right;
    v3 up;
    v3 look;

    // Cache frustum properties.
    f32 near_z;
    f32 far_z;
    f32 aspect;
    f32 fov_y;
    f32 near_window_height;
    f32 far_window_height;

    // Cache View/Proj matrices.
    m4 view;
    m4 proj;

    XMVECTOR get_position_as_xmvector() const;
    void set_lens(f32 fovY, f32 p_aspect, f32 zn, f32 zf);
    void set_lens_ortho(f32 width, f32 height, f32 zn, f32 zf);
    void look_at(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
    void look_at(const v3 &pos, const v3 &target, const v3 &p_up);
    XMMATRIX get_view_as_xmmatrix() const;
    XMMATRIX get_proj_as_xmmatrix() const;
    XMMATRIX get_viewproj_as_xmmatrix() const;
    void rotate_y(f32 angle);
    void update_view_matrix();
};

Camera camera_new();

// controls the camera as an orbital around a point
// mouse controls
struct CameraControlsOrbital {
    f32 cam_y_angle;
    f32 cam_x_angle;
    f32 cam_distance;
    f32 cam_auto_speed;

    f32 camera_center[3];

    bool auto_rotate;
    bool imgui_active;

    void init();

    void tick(Camera &camera, Input &input, f32 dt_sec, bool draw_imgui = false);

    void tick_auto(Camera &camera, f32 dt_sec);
    void tick_manual(Camera &camera, Input &input);
    void tick_math(Camera &camera);
};