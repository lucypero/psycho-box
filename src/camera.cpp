#include "camera.hpp"
#include "utils.hpp"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

Camera camera_new() {

    Camera cam = {};

    cam.position = v3(0.0f, 0.0f, 0.0f);
    cam.right = v3(1.0f, 0.0f, 0.0f);
    cam.up = v3(0.0f, 1.0f, 0.0f);
    cam.look = v3(0.0f, 0.0f, 1.0f);

    cam.set_lens(0.25f * math::Pi, 1.0f, 1.0f, 1000.0f);

    return cam;
}

XMVECTOR Camera::get_position_as_xmvector() const {
    return XMLoadFloat3(&position);
}

void Camera::set_lens(f32 fovY, f32 p_aspect, f32 zn, f32 zf) {
    // cache properties
    fov_y = fovY;
    aspect = p_aspect;
    near_z = zn;
    far_z = zf;

    near_window_height = 2.0f * near_z * tanf(0.5f * fov_y);
    far_window_height = 2.0f * far_z * tanf(0.5f * fov_y);

    XMMATRIX P = XMMatrixPerspectiveFovLH(fov_y, aspect, near_z, far_z);
    XMStoreFloat4x4(&proj, P);
}

void Camera::set_lens_ortho(f32 width, f32 height, f32 zn, f32 zf) {

    // cache properties
    aspect = width / height;
    near_z = zn;
    far_z = zf;

    near_window_height = 2.0f * near_z * tanf(0.5f * fov_y);
    far_window_height = 2.0f * far_z * tanf(0.5f * fov_y);

    XMMATRIX P = XMMatrixOrthographicLH(width, height, zn, zf);
    XMStoreFloat4x4(&proj, P);
}

void Camera::look_at(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp) {
    XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
    XMVECTOR U = XMVector3Cross(L, R);

    XMStoreFloat3(&position, pos);
    XMStoreFloat3(&look, L);
    XMStoreFloat3(&right, R);
    XMStoreFloat3(&up, U);
}

void Camera::look_at(const v3 &pos, const v3 &target, const v3 &p_up) {
    XMVECTOR P = XMLoadFloat3(&pos);
    XMVECTOR T = XMLoadFloat3(&target);
    XMVECTOR U = XMLoadFloat3(&p_up);

    look_at(P, T, U);
}

XMMATRIX Camera::get_view_as_xmmatrix() const {
    return XMLoadFloat4x4(&view);
}

XMMATRIX Camera::get_proj_as_xmmatrix() const {
    return XMLoadFloat4x4(&proj);
}

XMMATRIX Camera::get_viewproj_as_xmmatrix() const {
    return XMMatrixMultiply(get_view_as_xmmatrix(), get_proj_as_xmmatrix());
}

void Camera::rotate_y(f32 angle) {
    // Rotate the basis vectors about the world y-axis.

    XMMATRIX R = XMMatrixRotationY(angle);

    XMStoreFloat3(&right, XMVector3TransformNormal(XMLoadFloat3(&right), R));
    XMStoreFloat3(&up, XMVector3TransformNormal(XMLoadFloat3(&up), R));
    XMStoreFloat3(&look, XMVector3TransformNormal(XMLoadFloat3(&look), R));
}

void Camera::update_view_matrix() {
    XMVECTOR R = XMLoadFloat3(&right);
    XMVECTOR U = XMLoadFloat3(&up);
    XMVECTOR L = XMLoadFloat3(&look);
    XMVECTOR P = XMLoadFloat3(&position);

    // Keep camera's axes orthogonal to each other and of unit length.
    L = XMVector3Normalize(L);
    U = XMVector3Normalize(XMVector3Cross(L, R));

    // U, L already ortho-normal, so no need to normalize cross product.
    R = XMVector3Cross(U, L);

    // Fill in the view matrix entries.
    f32 x = -XMVectorGetX(XMVector3Dot(P, R));
    f32 y = -XMVectorGetX(XMVector3Dot(P, U));
    f32 z = -XMVectorGetX(XMVector3Dot(P, L));

    XMStoreFloat3(&right, R);
    XMStoreFloat3(&up, U);
    XMStoreFloat3(&look, L);

    view(0, 0) = right.x;
    view(1, 0) = right.y;
    view(2, 0) = right.z;
    view(3, 0) = x;

    view(0, 1) = up.x;
    view(1, 1) = up.y;
    view(2, 1) = up.z;
    view(3, 1) = y;

    view(0, 2) = look.x;
    view(1, 2) = look.y;
    view(2, 2) = look.z;
    view(3, 2) = z;

    view(0, 3) = 0.0f;
    view(1, 3) = 0.0f;
    view(2, 3) = 0.0f;
    view(3, 3) = 1.0f;
}

void CameraControlsOrbital::tick_auto(Camera &camera, f32 dt_sec) {
    this->cam_y_angle += dt_sec * this->cam_auto_speed;
    this->tick_math(camera);
}

void CameraControlsOrbital::tick_math(Camera &camera) {
    // the math part

    v3 radius_point_f(0.0f, 0.0f, -this->cam_distance);
    v3 cam_center_f(this->camera_center);

    XMVECTOR radius_point = XMLoadFloat3(&radius_point_f);
    XMVECTOR cam_center = XMLoadFloat3(&cam_center_f);

    XMVECTOR the_point = cam_center + radius_point;

    XMVECTOR translated_point = the_point - cam_center;

    XMMATRIX R = XMMatrixRotationRollPitchYaw(this->cam_x_angle, this->cam_y_angle, 0.0f);

    XMVECTOR transformed_point = XMVector3Transform(translated_point, R);

    // translate the point back to the original position
    transformed_point += cam_center;

    v3 final_cam_pos;
    XMStoreFloat3(&final_cam_pos, transformed_point);

    v3 cam_up(0.0f, 1.0f, 0.0f);
    camera.look_at(final_cam_pos, cam_center_f, cam_up);
    camera.update_view_matrix();
}

void CameraControlsOrbital::init() {
    *this = {};
    this->cam_auto_speed = 3.0f;
    this->cam_distance = 5.0f;
    this->cam_x_angle = 0.804249f;
}

void CameraControlsOrbital::tick_manual(Camera &camera, Input &i) {

    // the input part

    // 1.0f would be 1 full turn per pixel of mouse movement (1 tau radians)
    const f32 turns_per_pixel = 0.001f;
    const f32 scroll_speed = 1.0f;

    if (i.mouse_moved() && i.is_down(Key::MOUSE_1)) {
        f32 dx = (f32)i.mouse_cur_pos_x - (f32)i.mouse_last_pos_x;
        f32 dy = (f32)i.mouse_cur_pos_y - (f32)i.mouse_last_pos_y;

        this->cam_y_angle -= math::Tau * (dx * turns_per_pixel);
        this->cam_x_angle -= math::Tau * (dy * turns_per_pixel);
    }

    if (i.was_up(Key::SCROLL_UP)) {
        this->cam_distance -= scroll_speed;
    }

    if (i.was_up(Key::SCROLL_DOWN)) {
        this->cam_distance += scroll_speed;
    }

    // constraining distance and angle
    this->cam_x_angle = math::clampf(this->cam_x_angle, -math::Tau * 0.20f, math::Tau * 0.20f);

    if (this->cam_distance <= 0.1f) {
        this->cam_distance = 0.1f;
    }

    this->tick_math(camera);
}

void CameraControlsOrbital::tick(Camera &camera, Input &i, f32 dt_sec, bool draw_imgui) {
    if (draw_imgui) {
#ifdef _DEBUG
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "cam orbital controls");

        ImGui::DragFloat3("Camera Center", this->camera_center, 0.01f, -300.0f, 300.0f);

        ImGui::DragFloat("camera distance", &this->cam_distance, 0.01f);
        ImGui::Checkbox("auto rotate cam", &this->auto_rotate);
#endif
    }

    if (this->auto_rotate) {
#ifdef _DEBUG
        if (draw_imgui) {
            ImGui::DragFloat("camera auto speed", &this->cam_auto_speed, 0.01f);
        }
#endif
        this->tick_auto(camera, dt_sec);
    } else {
        this->tick_manual(camera, i);
    }
}
