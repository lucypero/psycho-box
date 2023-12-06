#pragma once

#define NOMINMAX
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <d3d11_1.h>
#include <windows.h>
#include <wrl/client.h>
#include <unordered_map>

#include "camera.hpp"
#include "input.hpp"
#include "lucytypes.hpp"
#include "utils.hpp"
#include "audio.hpp"

using Microsoft::WRL::ComPtr;

using Context = ID3D11DeviceContext;
using Device = ID3D11Device;

struct App;

struct DirectionalLight {
    v4 Ambient;
    v4 Diffuse;
    v4 Specular;
    v3 Direction;
    float Pad;
};

struct Material {
    v4 Ambient;
    v4 Diffuse;
    v4 Specular; // w = SpecPower
    v4 Reflect;
};

namespace Shaders {
struct Basic {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
};
}; // namespace Shaders

namespace Vertex {
// Basic 32-byte vertex structure.
struct Basic {
    v3 pos;
    v3 normal;
    v2 tex;
};
}; // namespace Vertex

namespace ConstantBuffers {

// register(b0)
struct FrameCBuffer {
    m4 vp;
    m4 view;
    m4 proj;
    m4 view_inv;
    m4 proj_inv;
    m4 wvp;
    DirectionalLight light;
    v3 eye_pos_w;
    f32 near_plane;
    f32 far_plane;
    f32 pad[3];
};

// register(b1)
struct ObjectCBuffer {
    Material mat;
};

// register(b0)
struct PostFXCBuffer {
    v2 res;
    f32 time;
    f32 pad;
};

}; // namespace ConstantBuffers

struct InstancedData {
    m4 world;
    m4 world_inv_transpose;
    m4 shadow_transform;
    v4 color;
};

const inline constexpr array shader_names = {"basic_instanced"sv, "text"sv,    "grid"sv,
                                             "shadowmap"sv,       "post_fx"sv, "background_fx"sv};

// This is just to make indexing the shader array more friendly.
//  has to correspond well to the order of shaders in shader_names
namespace ShaderName {
enum ShaderName : u32 { Basic, Text, Grid, Shadowmap, PostFx, BackgroundFx };
};

struct Resources {
    array<Shaders::Basic, shader_names.size()> shaders;

    ComPtr<ID3D11RasterizerState> rs_wireframe;
    ComPtr<ID3D11RasterizerState> rs_no_cull;
    ComPtr<ID3D11RasterizerState> rs_shadowmap;

    ComPtr<ID3D11BlendState> bs_alpha_to_coverage;
    ComPtr<ID3D11BlendState> bs_transparent;

    ComPtr<ID3D11DepthStencilState> dss_ignore_depth_buffer;
    ComPtr<ID3D11DepthStencilState> dss_depth_no_write;

    array<D3D11_INPUT_ELEMENT_DESC, 3> ied_basic;
    array<D3D11_INPUT_ELEMENT_DESC, 16> ied_instanced_basic;

    ComPtr<ID3D11InputLayout> il_basic;
    ComPtr<ID3D11InputLayout> il_instanced_basic;

    ComPtr<ID3D11SamplerState> ss_basic;
    ComPtr<ID3D11SamplerState> ss_anisotropic;
    ComPtr<ID3D11SamplerState> ss_shadow;

    ComPtr<ID3D11Buffer> cb_frame;
    ComPtr<ID3D11Buffer> cb_object;
    ComPtr<ID3D11Buffer> cb_postfx; // buffer to use in post processing effects
};

struct Glyph {
    // uvs of the glyph in the atlas for each vertex of quad.
    //      [top_left, top_right, bottom_left, bottom_right]
    v2 uvs[4];
    u32 size[2];
    i32 bearing[2];
    u32 advance;
};

struct Font {
    std::unordered_map<char, Glyph> font_map;
    ComPtr<ID3D11Texture2D> font_atlas_tex;
    ComPtr<ID3D11ShaderResourceView> font_atlas_srv;
};

enum struct ShapeKind : u32 {
    Box,
    Sphere,
    TriangularPrism,
    Goal,
    BoxPlayer,
    Count,
};

struct ShapeCountOffset {
    u32 count;
    u32 offset;
    u32 vert_offset;
};

using ShapeCountOffsets = array<ShapeCountOffset, (size_t)ShapeKind::Count>;

struct Shape {
    v3 pos;
    v3 scale = math::v3_one();
    v4 rot; // axis on rot.xyz + angle on rot.w
    v4 color = math::v4_one();
    ShapeKind kind = ShapeKind::Box;
};

struct TextString {
    string the_text;
    v2 text_offset; // offset from the middle of the screen i think
    f32 text_scale;
    bool centered;
};

struct Renderer {
    HINSTANCE hinstance;
    HWND hwindow;

    f64 seconds_per_count;

    int client_width;
    int client_height;
    bool minimized;
    bool maximized;
    bool resizing;

    ComPtr<Device> device;
    ComPtr<Context> context;

    // swapchain
    ComPtr<IDXGISwapChain1> swapchain;
    ComPtr<ID3D11Texture2D> sc_render_target_buffer;
    ComPtr<ID3D11RenderTargetView> sc_render_target_view;
    D3D11_VIEWPORT screen_viewport;

    Resources resources;

    UINT msaa_quality;
    ComPtr<ID3D11Texture2D> render_target_buffer;
    ComPtr<ID3D11Texture2D> render_target_buffer_non_ms;
    ComPtr<ID3D11RenderTargetView> render_target_view;
    ComPtr<ID3D11ShaderResourceView> render_target_srv;
    ComPtr<ID3D11Texture2D> depth_stencil_buffer;
    ComPtr<ID3D11DepthStencilView> depth_stencil_view;

    ComPtr<ID3D11Buffer> instanced_buffer;

    ComPtr<ID3D11Buffer> shapes_vb;
    ComPtr<ID3D11Buffer> shapes_ib;
    ShapeCountOffsets shapes_count_offset;

    ComPtr<ID3D11Buffer> text_vb;
    Font font;

    DirectionalLight light;
    Camera camera;
    Camera text_camera;
    Material mat;
    v3 camera_target;

    // textures for the 3D meshes
    ComPtr<ID3D11ShaderResourceView> color_floor_srv;
    ComPtr<ID3D11ShaderResourceView> color_border_srv;
    ComPtr<ID3D11ShaderResourceView> specular_srv;

    // shadowmap
    ComPtr<ID3D11ShaderResourceView> depth_map_srv;
    ComPtr<ID3D11DepthStencilView> depth_map_dsv;
    D3D11_VIEWPORT depth_map_viewport;

    // immediate mode "state" (gets cleared at the end of every frame)
    vec<Shape> shapes_opaque;
    vec<Shape> shapes_transparent;
    vec<TextString> text_strings;
    bool draw_grid;
};

namespace rend {

void clear_frame(Renderer &renderer);
void present_frame(Renderer &renderer);
void clean_up();

void draw_text(Renderer &renderer, string_view text, f32 text_scale, v2 text_offset);
// offset from the middle of the screen i think
void draw_text(Renderer &renderer, string_view text, f32 text_scale, v2 text_offset, bool centered);
void draw_grid(Renderer &renderer);

void toggle_fullscreen(Renderer &renderer);

// it only sets light at the frame level (same light for everything)
void set_light(Renderer &renderer, DirectionalLight light);
// it only sets material at the frame level (same material for everything)
void set_material(Renderer &renderer, Material mat);
void set_camera(Renderer &renderer, Camera cam);
// this is just for the shadowmap
void set_camera_target_pos(Renderer &renderer, v3 target);
void set_text_camera(Renderer &renderer, Camera cam);
void draw_shape(Renderer &renderer, Shape shape);
void construct_frame(Renderer &renderer);

} // namespace rend

struct Ctx {
    Input *input;
    App *app;
    Renderer *renderer;
    AudioData *audio;
    bool exit = false;
};

bool create_window(Ctx &ctx, HINSTANCE hInstance);
bool init_renderer(Ctx &ctx);

#ifdef _DEBUG
void compile_all_shaders_to_files();
#endif