#include "renderer.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include "ft2build.h"
#include FT_FREETYPE_H

#ifdef _DEBUG
#include <d3dcompiler.h>
#include "imgui\imgui.h"
#include "imgui\backends\imgui_impl_win32.h"
#include "imgui\backends\imgui_impl_dx11.h"
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <assimp/Importer.hpp>  // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postprocess.h> // Post processing flags

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244)
#include "stb_image.h"
#pragma warning(pop)

#include "app.hpp"

using namespace DirectX;

namespace {

const int CLIENT_WIDTH_INITIAL = 1500;
const int CLIENT_HEIGHT_INITIAL = 800;

const aiScene *assimp_read_file(Assimp::Importer &importer, string_view filename) {

    const aiScene *scene = importer.ReadFile(
        string(filename), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                              aiProcess_SortByPType | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder);

    if (scene == nullptr) {
        lassert(false);
    }

    return scene;
}

void internal_draw_shapes(Renderer &renderer, Context &c, u32 current_shape_count, u32 current_shape_offset,
                          u32 current_shape_kind) {
    {
        array<ID3D11ShaderResourceView *, 1> srvs;

        // horrible hack. it binds the floor texture when it's time to render floors.
        //   otherwise it uses the border texture.
        if (current_shape_kind == (u32)ShapeKind::Box) {
            srvs[0] = renderer.color_floor_srv.Get();
        } else {
            srvs[0] = renderer.color_border_srv.Get();
        }

        c.PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
    }

    const ShapeCountOffset &co = renderer.shapes_count_offset[current_shape_kind];
    c.DrawIndexedInstanced(co.count, current_shape_count, co.offset, co.vert_offset, current_shape_offset);
};

// given an image file, it creates a texture2d, a srv, and generates all the mipmaps.
void create_texture_from_image(const char *filename, Renderer &r, ComPtr<ID3D11ShaderResourceView> &srv) {
    int image_width;
    int image_height;
    int image_channels;
    int image_desired_channels = 4;

    unsigned char *image_data =
        stbi_load(filename, &image_width, &image_height, &image_channels, image_desired_channels);
    lassert(image_data);

    int image_pitch = image_width * 4;

    // texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 0; // set to 0 to gen all the mips
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> image_texture;
    r.device->CreateTexture2D(&desc, nullptr, image_texture.ReleaseAndGetAddressOf());

    // srv
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = (UINT)-1; // Use all mip levels

    r.device->CreateShaderResourceView(image_texture.Get(), &srv_desc, srv.ReleaseAndGetAddressOf());

    // copying image date to texture
    r.context->UpdateSubresource(image_texture.Get(), 0, nullptr, image_data, image_pitch, 0);
    free(image_data);

    // gen mips
    r.context->GenerateMips(srv.Get());
}

void on_resize(Ctx &ctx) {

    Renderer &r = *ctx.renderer;

    lassert(r.context);
    lassert(r.device);
    lassert(r.swapchain);

    // Release the old views, as they hold references to the buffers we
    // will be destroying.  Also release the old depth/stencil buffer.
    // r.sc_depth_stencil_buffer.

    r.sc_render_target_buffer.Reset();
    r.sc_render_target_view.Reset();

    // Resize the swap chain and recreate the render target view.
    r.swapchain->ResizeBuffers(2, r.client_width, r.client_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    r.swapchain->GetBuffer(0, IID_PPV_ARGS(r.sc_render_target_buffer.ReleaseAndGetAddressOf()));
    r.device->CreateRenderTargetView(r.sc_render_target_buffer.Get(), 0,
                                     r.sc_render_target_view.ReleaseAndGetAddressOf());

    // Create the depth/stencil buffer and view.

    // Set the viewport transform.
    r.screen_viewport.TopLeftX = 0;
    r.screen_viewport.TopLeftY = 0;
    r.screen_viewport.Width = static_cast<float>(r.client_width);
    r.screen_viewport.Height = static_cast<float>(r.client_height);
    r.screen_viewport.MinDepth = 0.0f;
    r.screen_viewport.MaxDepth = 1.0f;
    r.context->RSSetViewports(1, &r.screen_viewport);

    // Creating render color buffer and depth stencil buffer for rendering (with
    // MSAA)

    // Color buffer and view
    D3D11_TEXTURE2D_DESC render_target_desc = {};
    render_target_desc.Width = r.client_width;
    render_target_desc.Height = r.client_height;
    render_target_desc.MipLevels = 1;
    render_target_desc.ArraySize = 1;
    render_target_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    render_target_desc.SampleDesc.Count = 4;
    render_target_desc.SampleDesc.Quality = r.msaa_quality - 1;
    render_target_desc.Usage = D3D11_USAGE_DEFAULT;
    render_target_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    r.device->CreateTexture2D(&render_target_desc, 0, r.render_target_buffer.ReleaseAndGetAddressOf());

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DMS,
                                                        DXGI_FORMAT_R8G8B8A8_UNORM);

    r.device->CreateRenderTargetView(r.render_target_buffer.Get(), &renderTargetViewDesc,
                                     r.render_target_view.ReleaseAndGetAddressOf());

    // -- Resources for Post Processing --
    {
        // texture2D. a non-MS texture copy of the main render target.
        //    Meant to be used as SRV for the Post FX shader.

        render_target_desc.SampleDesc.Count = 1;
        render_target_desc.SampleDesc.Quality = 0;
        render_target_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        r.device->CreateTexture2D(&render_target_desc, 0, r.render_target_buffer_non_ms.ReleaseAndGetAddressOf());

        // srv for use when post processing
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = render_target_desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        r.device->CreateShaderResourceView(r.render_target_buffer_non_ms.Get(), &srv_desc,
                                           r.render_target_srv.ReleaseAndGetAddressOf());
    }

    // depth stencil
    D3D11_TEXTURE2D_DESC depthStencilDesc = {};

    depthStencilDesc.Width = r.client_width;
    depthStencilDesc.Height = r.client_height;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 4;
    depthStencilDesc.SampleDesc.Quality = r.msaa_quality - 1;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    r.device->CreateTexture2D(&depthStencilDesc, 0, r.depth_stencil_buffer.ReleaseAndGetAddressOf());
    r.device->CreateDepthStencilView(r.depth_stencil_buffer.Get(), 0,
                                     r.depth_stencil_view.ReleaseAndGetAddressOf());

    app_on_resize(*ctx.app, r);
}

void on_mouse_down(Ctx &ctx, WPARAM button_state, i32 x, i32 y) {
    SetCapture(ctx.renderer->hwindow);
    ctx.input->mouse_cur_pos_x = x;
    ctx.input->mouse_cur_pos_y = y;

    // capturing mouse button state
    ctx.input->keys[(size_t)Key::MOUSE_1] = (button_state & MK_LBUTTON) != 0;
    ctx.input->keys[(size_t)Key::MOUSE_2] = (button_state & MK_RBUTTON) != 0;
}

void on_mouse_up(Ctx &ctx, WPARAM button_state) {
    ReleaseCapture();
    ctx.input->keys[(size_t)Key::MOUSE_1] = (button_state & MK_LBUTTON) != 0;
    ctx.input->keys[(size_t)Key::MOUSE_2] = (button_state & MK_RBUTTON) != 0;
}

void on_mouse_move(Ctx &ctx, i32 x, i32 y) {
    ctx.input->mouse_cur_pos_x = x;
    ctx.input->mouse_cur_pos_y = y;
}

void register_key(Input &input, WPARAM key_param, bool is_down) {

    // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

    Key key = Key::COUNT;

    switch (key_param) {
    case 0x41:
        key = Key::A;
        break;
    case 0x42:
        key = Key::B;
        break;
    case 0x43:
        key = Key::C;
        break;
    case 0x44:
        key = Key::D;
        break;
    case 0x45:
        key = Key::E;
        break;
    case 0x46:
        key = Key::F;
        break;
    case 0x47:
        key = Key::G;
        break;
    case 0x48:
        key = Key::H;
        break;
    case 0x49:
        key = Key::I;
        break;
    case 0x4A:
        key = Key::J;
        break;
    case 0x4B:
        key = Key::K;
        break;
    case 0x4C:
        key = Key::L;
        break;
    case 0x4D:
        key = Key::M;
        break;
    case 0x4E:
        key = Key::N;
        break;
    case 0x4F:
        key = Key::O;
        break;
    case 0x50:
        key = Key::P;
        break;
    case 0x51:
        key = Key::Q;
        break;
    case 0x52:
        key = Key::R;
        break;
    case 0x53:
        key = Key::S;
        break;
    case 0x54:
        key = Key::T;
        break;
    case 0x55:
        key = Key::U;
        break;
    case 0x56:
        key = Key::V;
        break;
    case 0x57:
        key = Key::W;
        break;
    case 0x58:
        key = Key::X;
        break;
    case 0x59:
        key = Key::Y;
        break;
    case 0x5A:
        key = Key::Z;
        break;
    case 0x30:
        key = Key::NUM_0;
        break;
    case 0x31:
        key = Key::NUM_1;
        break;
    case 0x32:
        key = Key::NUM_2;
        break;
    case 0x33:
        key = Key::NUM_3;
        break;
    case 0x34:
        key = Key::NUM_4;
        break;
    case 0x35:
        key = Key::NUM_5;
        break;
    case 0x36:
        key = Key::NUM_6;
        break;
    case 0x37:
        key = Key::NUM_7;
        break;
    case 0x38:
        key = Key::NUM_8;
        break;
    case VK_SPACE:
        key = Key::SPACE;
        break;
    case VK_ESCAPE:
        key = Key::ESCAPE;
        break;
    case VK_RETURN:
        key = Key::ENTER;
        break;
    case 0x39:
        key = Key::NUM_9;
        break;
    case VK_NUMPAD0:
        key = Key::NP_0;
        break;
    case VK_NUMPAD1:
        key = Key::NP_1;
        break;
    case VK_NUMPAD2:
        key = Key::NP_2;
        break;
    case VK_NUMPAD3:
        key = Key::NP_3;
        break;
    case VK_NUMPAD4:
        key = Key::NP_4;
        break;
    case VK_NUMPAD5:
        key = Key::NP_5;
        break;
    case VK_NUMPAD6:
        key = Key::NP_6;
        break;
    case VK_NUMPAD7:
        key = Key::NP_7;
        break;
    case VK_NUMPAD8:
        key = Key::NP_8;
        break;
    case VK_NUMPAD9:
        key = Key::NP_9;
        break;
    case VK_LEFT:
        key = Key::LEFT;
        break;
    case VK_DOWN:
        key = Key::DOWN;
        break;
    case VK_UP:
        key = Key::UP;
        break;
    case VK_RIGHT:
        key = Key::RIGHT;
        break;
    case VK_OEM_4:
        key = Key::SQUARE_LEFT;
        break;
    case VK_OEM_6:
        key = Key::SQUARE_RIGHT;
        break;
    }

    if (key == Key::COUNT) {
        return;
    }

    input.keys[(size_t)key] = is_down;
}

// Function to toggle between windowed and borderless fullscreen modes
void toggle_fullscreen_internal(HWND hWnd) {
    static WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    static DWORD dwStyle;
    static RECT rc;

    if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) {
        // Set windowed
        SetWindowLong(hWnd, GWL_STYLE, dwStyle);
        SetWindowPos(hWnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        // Save current window position and style
        dwStyle = GetWindowLong(hWnd, GWL_STYLE);
        GetWindowRect(hWnd, &rc);

        // Set borderless Fullscreen
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
        SetWindowPos(hWnd, HWND_TOP, 0, 0, w, h, SWP_FRAMECHANGED);
    }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    bool do_imgui_handling = false;

#ifdef _DEBUG
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
            do_imgui_handling = true;
        }
    }
#endif

    Ctx *ctx_p = (Ctx *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!ctx_p) {
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    Ctx &ctx = *ctx_p;
    Renderer *renderer = ctx.renderer;

    LRESULT result = 0;

    auto x_lparam = ((int)(short)LOWORD(lparam));
    auto y_lparam = ((int)(short)HIWORD(lparam));

    // early returns on some events when imgui is focused
    if (do_imgui_handling) {
        switch (msg) {
        case WM_KEYDOWN: {
            if (wparam == VK_ESCAPE)
                DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            // rctx->last_mouse_pos.x = GET_X_LPARAM(lparam);
            // rctx->last_mouse_pos.y = GET_Y_LPARAM(lparam);
            return 0;
        case WM_KEYUP:
        case WM_MOUSEWHEEL:
            return 0;
        }
    }

    switch (msg) {
    // WM_ACTIVATE is sent when the window is activated or deactivated.
    // We pause the game when the window is deactivated and unpause it
    // when it becomes active.
    case WM_ACTIVATE: {
        if (LOWORD(wparam) == WA_INACTIVE) {
            // ctx.app_paused = true;
        } else {
            // ctx.app_paused = false;
        }
        break;
    }
    // WM_SIZE is sent when the user resizes the window.
    case WM_SIZE: {
        // Save the new client area dimensions.
        renderer->client_width = LOWORD(lparam);
        renderer->client_height = HIWORD(lparam);
        if (renderer->device) {
            if (wparam == SIZE_MINIMIZED) {
                // renderer->app_paused = true;
                renderer->minimized = true;
                renderer->maximized = false;
            } else if (wparam == SIZE_MAXIMIZED) {
                // renderer->app_paused = false;
                renderer->minimized = false;
                renderer->maximized = true;
                on_resize(ctx);
            } else if (wparam == SIZE_RESTORED) {
                // Restoring from minimized state?
                if (renderer->minimized) {
                    // renderer->app_paused = false;
                    renderer->minimized = false;
                    on_resize(ctx);
                }

                // Restoring from maximized state?
                else if (renderer->maximized) {
                    // renderer->app_paused = false;
                    renderer->maximized = false;
                    on_resize(ctx);
                } else if (renderer->resizing) {
                    // If user is dragging the resize bars, we do not resize
                    // the buffers here because as the user continuously
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is
                    // done resizing the window and releases the resize bars,
                    // which sends a WM_EXITSIZEMOVE message.
                } else // API call such as SetWindowPos or
                       // renderer->swapchain->SetFullscreenState.
                {
                    on_resize(ctx);
                }
            }
        }
        break;
    }
    // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
    case WM_ENTERSIZEMOVE: {
        // renderer->app_paused = true;
        renderer->resizing = true;
        break;
    }
    // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
    // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE: {
        // renderer->app_paused = false;
        renderer->resizing = false;
        on_resize(ctx);
        break;
    }
    // The WM_MENUCHAR message is sent when a menu is active and the user
    // presses a key that does not correspond to any mnemonic or accelerator
    // key.
    case WM_MENUCHAR: {
        // Don't beep when we alt-enter.
        result = MAKELRESULT(0, MNC_CLOSE);
        break;
    }
    // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO: {
        ((MINMAXINFO *)lparam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO *)lparam)->ptMinTrackSize.y = 200;
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        on_mouse_down(ctx, wparam, x_lparam, y_lparam);
        break;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
        on_mouse_up(ctx, wparam);
        break;
    }
    case WM_MOUSEMOVE: {
        on_mouse_move(ctx, x_lparam, y_lparam);
        break;
    }
    case WM_MOUSEWHEEL: {
        // scroll up is positive, down is negative
        i32 z_delta = ((i32)(short)HIWORD(wparam));
        if (z_delta > 0) {
            ctx.input->keys[(size_t)Key::SCROLL_UP] = true;
        } else if (z_delta < 0) {
            ctx.input->keys[(size_t)Key::SCROLL_DOWN] = true;
        }
        break;
    }
    case WM_KEYDOWN: {
        register_key(*ctx.input, wparam, true);
        break;
    }
    case WM_KEYUP: {
        register_key(*ctx.input, wparam, false);
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default: {
        result = DefWindowProcA(hwnd, msg, wparam, lparam);
        break;
    }
    }

    return result;
}

bool init_font_renderer(Device &device, Font &font) {

    // we'll try to make an atlas texture w all the ascii characters.
    font.font_map.clear();

    FT_Library ft;
    auto error = FT_Init_FreeType(&ft);
    if (error) {
        return false;
    }

    FT_Face face;

    error = FT_New_Face(ft, "assets/fonts/Cabin-Bold.ttf", 0, &face);

    if (error) {
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, 50);

    const i32 padding = 5;

    u32 total_width = padding;
    u32 total_height = padding * 2;

    // looping through all the ascii characters
    for (char c = 33; c <= 126; ++c) {

        error = FT_Load_Char(face, c, FT_LOAD_RENDER);
        lassert(!error);

        u32 w = face->glyph->bitmap.width;
        u32 h = face->glyph->bitmap.rows;

        total_width += w + padding;
        total_height = math::Max(total_height, h + padding * 2);
    }

    vec<u8> atlas_buffer(total_width * total_height);

    i32 width_offset = padding;

    for (char c = 32; c <= 126; ++c) {

        error = FT_Load_Char(face, c, FT_LOAD_RENDER);
        lassert(!error);

        u32 w = face->glyph->bitmap.width;
        u32 h = face->glyph->bitmap.rows;

        // filling row per row on the atlas
        for (i32 i = 0; i < (i32)h; ++i) {
            memcpy(&atlas_buffer[(i + padding) * total_width + width_offset], face->glyph->bitmap.buffer + i * w,
                   w);
        }

        // constructing glyph
        Glyph g = {};
        g.size[0] = w;
        g.size[1] = h;
        g.bearing[0] = face->glyph->bitmap_left;
        g.bearing[1] = face->glyph->bitmap_top;
        g.advance = face->glyph->advance.x;
        // top left
        g.uvs[0] = v2((f32)width_offset / (f32)total_width, (f32)padding / (f32)total_height);
        // top right
        g.uvs[1] = v2((f32)(width_offset + w) / (f32)total_width, (f32)padding / (f32)total_height);
        // bottom left
        g.uvs[2] = v2((f32)width_offset / (f32)total_width, (f32)(padding + h) / (f32)total_height);
        // bottom right
        g.uvs[3] = v2((f32)(width_offset + w) / (f32)total_width, (f32)(padding + h) / (f32)total_height);

        font.font_map[c] = g;

        width_offset += w + padding;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // creating the DX texture.

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = total_width;
    tex_desc.Height = total_height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = &(atlas_buffer[0]);
    init_data.SysMemPitch = total_width * 1;
    init_data.SysMemSlicePitch = 0;

    // Create the texture
    device.CreateTexture2D(&tex_desc, &init_data, font.font_atlas_tex.ReleaseAndGetAddressOf());

    // Create the SRV
    device.CreateShaderResourceView(font.font_atlas_tex.Get(), nullptr,
                                    font.font_atlas_srv.ReleaseAndGetAddressOf());

    return true;
}

enum struct ShaderType { VS, PS };

#ifdef _DEBUG

void compile_shader(span<u8> file, ShaderType st, vec<u8> &bin, string_view source_filename, bool optimize) {

    u32 compile_flags = 0;

    if (!optimize) {
        compile_flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL0 |
                        D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
    } else {
        compile_flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    }

    char entry_point[20];
    char target[20];

    // gotta allocate a string to gen the c_str();
    string source_filename_str = string(source_filename);
    const char *source_filename_cstr = source_filename_str.c_str();

    switch (st) {
    case ShaderType::VS:
        strcpy_s(entry_point, "mainVS");
        strcpy_s(target, "vs_5_0");
        break;
    case ShaderType::PS:
    default:
        strcpy_s(entry_point, "mainPS");
        strcpy_s(target, "ps_5_0");
        break;
    }

    ID3DBlob *shader_errors = 0;

    ID3DBlob *bin_blob = 0;

    HRESULT res =
        D3DCompile2(file.data(), file.size(), source_filename_cstr, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    entry_point, target, compile_flags, 0, 0, nullptr, 0, &bin_blob, &shader_errors);

    if (shader_errors != nullptr) {
        log("shader compilation errors found for %s:", source_filename_cstr);
        char *error_buf = (char *)malloc(sizeof(char) * shader_errors->GetBufferSize() + 1);
        memcpy(error_buf, shader_errors->GetBufferPointer(), shader_errors->GetBufferSize());
        error_buf[shader_errors->GetBufferSize()] = '\0';
        log("%s", error_buf);
        lassert(false);
    }

    const u8 *shader_bytes = static_cast<const uint8_t *>(bin_blob->GetBufferPointer());
    vec<u8> bin_ret(shader_bytes, shader_bytes + bin_blob->GetBufferSize());
    bin = bin_ret;

    bin_blob->Release();

    lassert(SUCCEEDED(res));
}

void compile_shader_vsps(string_view source_filename, vec<u8> &vs_bin, vec<u8> &ps_bin, bool optimize) {

    vec<u8> hlsl_file = load_file(source_filename);

    compile_shader(hlsl_file, ShaderType::VS, vs_bin, source_filename, optimize);
    compile_shader(hlsl_file, ShaderType::PS, ps_bin, source_filename, optimize);
}

#endif

void basic_shader_init(Shaders::Basic &s, Device &d, span<u8> blob_vs, span<u8> blob_ps) {
    d.CreateVertexShader(blob_vs.data(), blob_vs.size(), nullptr, s.vs.ReleaseAndGetAddressOf());
    d.CreatePixelShader(blob_ps.data(), blob_ps.size(), nullptr, s.ps.ReleaseAndGetAddressOf());
}

bool render_resources_init(Device &d, Resources &res) {

    // Loading shaders and creating Input Layouts
    res.ied_basic = {{{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
                       D3D11_INPUT_PER_VERTEX_DATA, 0},
                      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
                       D3D11_INPUT_PER_VERTEX_DATA, 0}}};

    res.ied_instanced_basic = {
        {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
          0},
         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD_INV_TRANSPOSE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD_INV_TRANSPOSE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD_INV_TRANSPOSE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"WORLD_INV_TRANSPOSE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"SHADOW_TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"SHADOW_TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"SHADOW_TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"SHADOW_TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1},
         {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_INSTANCE_DATA, 1}}};

    for (u32 i = 0; const auto &shader : shader_names) {
#ifdef _DEBUG
        vec<u8> vs_b = {};
        vec<u8> ps_b = {};
        compile_shader_vsps(format("src/shaders/{}.hlsl", shader), vs_b, ps_b, false);
#else
        vec<u8> vs_b = load_file(format("assets/shader_bin/{}.vs.cso", shader));
        vec<u8> ps_b = load_file(format("assets/shader_bin/{}.ps.cso", shader));
#endif
        basic_shader_init(res.shaders[i], d, vs_b, ps_b);

        // Creating input layouts
        if (i == ShaderName::Text) {
            d.CreateInputLayout(res.ied_basic.data(), (u32)res.ied_basic.size(), vs_b.data(), vs_b.size(),
                                res.il_basic.ReleaseAndGetAddressOf());
        } else if (i == ShaderName::Basic) {
            d.CreateInputLayout(res.ied_instanced_basic.data(), (u32)res.ied_instanced_basic.size(), vs_b.data(),
                                vs_b.size(), res.il_instanced_basic.ReleaseAndGetAddressOf());
        }

        ++i;
    }

    // creating Render States
    {
        D3D11_RASTERIZER_DESC rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        rasterizer_desc.FrontCounterClockwise = false;
        rasterizer_desc.DepthClipEnable = true;

        d.CreateRasterizerState(&rasterizer_desc, res.rs_wireframe.ReleaseAndGetAddressOf());

        rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_NONE;
        rasterizer_desc.FrontCounterClockwise = false;
        rasterizer_desc.DepthClipEnable = true;

        d.CreateRasterizerState(&rasterizer_desc, res.rs_no_cull.ReleaseAndGetAddressOf());

        // render state for rendering shadowmap

        rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        rasterizer_desc.DepthBias = 100000;
        rasterizer_desc.DepthBiasClamp = 0.0f;
        rasterizer_desc.SlopeScaledDepthBias = 1.0f;
        rasterizer_desc.FrontCounterClockwise = false;
        rasterizer_desc.DepthClipEnable = true;

        d.CreateRasterizerState(&rasterizer_desc, res.rs_shadowmap.ReleaseAndGetAddressOf());
    }

    // creating depth stencil states
    {
        D3D11_DEPTH_STENCIL_DESC ds_desc = {};
        ds_desc.DepthEnable = false;

        d.CreateDepthStencilState(&ds_desc, res.dss_ignore_depth_buffer.ReleaseAndGetAddressOf());

        ds_desc = {};
        ds_desc.DepthEnable = true;
        ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ds_desc.DepthFunc = D3D11_COMPARISON_LESS;

        d.CreateDepthStencilState(&ds_desc, res.dss_depth_no_write.ReleaseAndGetAddressOf());
    }

    // creating blend states
    {
        D3D11_BLEND_DESC blend_desc = {};
        blend_desc.AlphaToCoverageEnable = true;
        blend_desc.IndependentBlendEnable = false;
        blend_desc.RenderTarget[0].BlendEnable = false;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        d.CreateBlendState(&blend_desc, res.bs_alpha_to_coverage.ReleaseAndGetAddressOf());

        blend_desc = {};
        blend_desc.AlphaToCoverageEnable = false;
        blend_desc.IndependentBlendEnable = false;

        blend_desc.RenderTarget[0].BlendEnable = true;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        d.CreateBlendState(&blend_desc, res.bs_transparent.ReleaseAndGetAddressOf());
    }

    // maybe this should go somewhere else
    // constant buffers
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(ConstantBuffers::FrameCBuffer);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        d.CreateBuffer(&buffer_desc, nullptr, res.cb_frame.ReleaseAndGetAddressOf());

        buffer_desc.ByteWidth = sizeof(ConstantBuffers::ObjectCBuffer);

        d.CreateBuffer(&buffer_desc, nullptr, res.cb_object.ReleaseAndGetAddressOf());

        buffer_desc.ByteWidth = sizeof(ConstantBuffers::PostFXCBuffer);
        d.CreateBuffer(&buffer_desc, nullptr, res.cb_postfx.ReleaseAndGetAddressOf());
    }

    // initting sampler states
    {
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        d.CreateSamplerState(&sampler_desc, res.ss_basic.ReleaseAndGetAddressOf());

        sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MaxAnisotropy = 16;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        d.CreateSamplerState(&sampler_desc, res.ss_anisotropic.ReleaseAndGetAddressOf());

        sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        d.CreateSamplerState(&sampler_desc, res.ss_shadow.ReleaseAndGetAddressOf());
    }

    return true;
}

void actual_render_text(Renderer &renderer, const TextString &ts) {
    auto &c = *renderer.context.Get();
    auto &res = renderer.resources;

    vec<Vertex::Basic> text_buf;

    // pos for vertex on the top left of the current quad
    f32 str_advance = 0.0f;

    for (u32 i = 0; i < ts.the_text.size(); ++i) {
        char char_i = ts.the_text[i];

        Glyph g = renderer.font.font_map[char_i];

        if (char_i == ' ') {
            str_advance += (f32)(g.advance >> 6);
            continue;
        }

        Vertex::Basic v1 = {};
        v1.pos = v3(str_advance + (f32)g.bearing[0], (f32)g.bearing[1], 0.0f);
        v1.normal = v3(0.0f, 0.0f, -1.0f);
        v1.tex = g.uvs[0];

        Vertex::Basic v2 = v1, v3 = v1, v4 = v1;
        v2.pos.x += g.size[0];
        v2.tex = g.uvs[1];

        v3.pos.y -= g.size[1];
        v3.tex = g.uvs[2];

        v4.pos.x = v2.pos.x;
        v4.pos.y = v3.pos.y;
        v4.tex = g.uvs[3];

        text_buf.push_back(v1);
        text_buf.push_back(v2);
        text_buf.push_back(v3);

        text_buf.push_back(v3);
        text_buf.push_back(v2);
        text_buf.push_back(v4);

        str_advance += (f32)(g.advance >> 6);
    }

    D3D11_MAPPED_SUBRESOURCE mapped_res = {};

    // sending text vertices to gpu
    if (text_buf.size() > 0) {
        c.Map(renderer.text_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &text_buf[0], sizeof(Vertex::Basic) * text_buf.size());
        c.Unmap(renderer.text_vb.Get(), 0);
    }

    v2 text_offset = ts.text_offset;

    if (ts.centered) {
        text_offset.x -= str_advance * 0.5f * ts.text_scale;
    }

    // setting frame cbuffer
    XMMATRIX text_scale_mat = XMMatrixScaling(ts.text_scale, ts.text_scale, 1.0f);
    XMMATRIX text_offset_mat = XMMatrixTranslation(text_offset.x, text_offset.y, 10.0f);

    XMMATRIX world = XMMatrixMultiply(text_scale_mat, text_offset_mat);
    XMMATRIX view = renderer.text_camera.get_view_as_xmmatrix();
    XMMATRIX proj = renderer.text_camera.get_proj_as_xmmatrix();

    // updating constant buffers

    // updating frame cbuffer (b0)
    {
        ConstantBuffers::FrameCBuffer wvp_data = {};
        // XMStoreFloat4x4(&wvp_data.vp, view * proj);
        XMStoreFloat4x4(&wvp_data.wvp, world * view * proj);
        wvp_data.eye_pos_w = renderer.text_camera.position;

        mapped_res = {};
        c.Map(res.cb_frame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &wvp_data, sizeof(wvp_data));
        c.Unmap(res.cb_frame.Get(), 0);
    }

    // setting rendering pipeline state

    c.IASetInputLayout(res.il_basic.Get());
    c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    {
        ID3D11Buffer *vbs[1] = {renderer.text_vb.Get()};
        u32 stride[1] = {sizeof(Vertex::Basic)};
        u32 offset[1] = {0};
        c.IASetVertexBuffers(0, 1, vbs, stride, offset);
    }
    // c.RSSetState(RenderStates::wireframe);
    f32 blend_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    c.OMSetBlendState(res.bs_transparent.Get(), blend_factor, 0xffffffff);
    c.OMSetDepthStencilState(0, 0);

    // setting shader resources
    c.VSSetConstantBuffers(0, 1, res.cb_frame.GetAddressOf());
    c.PSSetConstantBuffers(0, 1, res.cb_frame.GetAddressOf());
    c.PSSetSamplers(0, 1, res.ss_basic.GetAddressOf());

    ID3D11ShaderResourceView *srvs[] = {renderer.font.font_atlas_srv.Get()};
    c.PSSetShaderResources(0, 1, srvs);

    // setting shaders
    c.VSSetShader(res.shaders[ShaderName::Text].vs.Get(), nullptr, 0);
    c.PSSetShader(res.shaders[ShaderName::Text].ps.Get(), nullptr, 0);

    // draw
    c.Draw((u32)text_buf.size(), 0);
}

} // namespace

// --------------------------------- EXPORTED FUNCTIONS (START) ---------------------------------

bool init_renderer(Ctx &ctx) {

    Renderer &r = *ctx.renderer;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(0, // default adapter
                                   D3D_DRIVER_TYPE_HARDWARE,
                                   0,                       // no software device
                                   createDeviceFlags, 0, 0, // default feature level array
                                   D3D11_SDK_VERSION, r.device.ReleaseAndGetAddressOf(), &featureLevel,
                                   r.context.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        MessageBoxA(0, "D3D11CreateDevice Failed.", 0, 0);
        return false;
    }

    if (featureLevel != D3D_FEATURE_LEVEL_11_0) {
        MessageBoxA(0, "Direct3D Feature Level 11 unsupported.", 0, 0);
        return false;
    }

#ifdef _DEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(r.device.As(&d3dDebug))) {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue))) {
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            D3D11_MESSAGE_ID hide[] = {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render
    // target formats, so we only need to check quality support.

    r.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &r.msaa_quality);
    lassert(r.msaa_quality > 0);

    // Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = r.client_width;
    sd.Height = r.client_height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;

    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // To correctly create the swap chain, we must use the IDXGIFactory that was
    // used to create the device.  If we tried to use a different IDXGIFactory
    // instance (by calling CreateDXGIFactory), we get an error:
    // "IDXGIFactory::CreateSwapChain: This function is being called with a
    // device from a different IDXGIFactory."

    IDXGIDevice *dxgiDevice = 0;
    r.device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);

    IDXGIAdapter *dxgiAdapter = 0;
    dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&dxgiAdapter);

    IDXGIFactory2 *dxgiFactory = 0;
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&dxgiFactory);

    dxgiFactory->CreateSwapChainForHwnd(r.device.Get(), r.hwindow, &sd, &fsSwapChainDesc, nullptr, &r.swapchain);

    dxgiDevice->Release();
    dxgiAdapter->Release();
    dxgiFactory->Release();

    // The remaining steps that need to be carried out for d3d creation
    // also need to be executed every time the window is resized.  So
    // just call the on_resize method here to avoid code duplication.

    on_resize(ctx);

    // initializing imgui

#ifdef _DEBUG
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("./assets/fonts/JetBrainsMonoNL-Medium.ttf", 20.0f);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(r.hwindow);
    ImGui_ImplDX11_Init(r.device.Get(), r.context.Get());
#endif

    // initting resources

    lassert_s(render_resources_init(*r.device.Get(), r.resources), "could not initialize render resources");

    // initing instanced buffer
    {
        const size_t max_box_count = 10000;
        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DYNAMIC;
        vbd.ByteWidth = (u32)(sizeof(InstancedData) * max_box_count);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        r.device->CreateBuffer(&vbd, 0, r.instanced_buffer.GetAddressOf());
    }

    Font font_demo = {};

    bool res_f = init_font_renderer(*r.device.Get(), font_demo);
    if (!res_f) {
        return false;
    }

    r.font = font_demo;

    // initting text vertex buffer
    const size_t max_glyph_quads = 500;

    {
        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DYNAMIC;
        vbd.ByteWidth = (u32)(sizeof(Vertex::Basic) * max_glyph_quads * 6);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        r.device->CreateBuffer(&vbd, 0, r.text_vb.GetAddressOf());
    }

    // creating all the shapes
    {
        const auto add_mesh = [](string_view filename, ShapeKind kind, ShapeCountOffsets &offsets,
                                 u32 &index_offset, u32 &vertex_offset, vec<Vertex::Basic> &vertices,
                                 vec<u32> &indices) {
            Assimp::Importer importer;

            auto scene = assimp_read_file(importer, filename);
            auto mesh = scene->mMeshes[0];

            offsets[(u32)kind] = ShapeCountOffset{
                .count = (u32)mesh->mNumFaces * 3, .offset = index_offset, .vert_offset = vertex_offset};

            vertices.resize(vertices.size() + mesh->mNumVertices);
            size_t i = vertex_offset;

            for (; i < vertex_offset + mesh->mNumVertices; ++i) {
                const auto vert_i = i - vertex_offset;

                vertices[i].pos =
                    v3(mesh->mVertices[vert_i].x, mesh->mVertices[vert_i].y, mesh->mVertices[vert_i].z);
                vertices[i].normal =
                    v3(mesh->mNormals[vert_i].x, mesh->mNormals[vert_i].y, mesh->mNormals[vert_i].z);
                vertices[i].tex = v2(mesh->mTextureCoords[0][vert_i].x, 1.0f - mesh->mTextureCoords[0][vert_i].y);
            }

            indices.resize(indices.size() + mesh->mNumFaces * 3);
            i = index_offset;

            for (; i < index_offset + mesh->mNumFaces * 3; ++i) {
                const auto index_i = i - index_offset;
                indices[i] = mesh->mFaces[index_i / 3].mIndices[index_i % 3];
            }

            index_offset += (u32)mesh->mNumFaces * 3;
            vertex_offset += (u32)mesh->mNumVertices;
        };

        u32 index_offset = 0;
        u32 vertex_offset = 0;

        vec<Vertex::Basic> vertices = {};
        vec<u32> indices = {};

        add_mesh("assets/models/cube.glb", ShapeKind::Box, r.shapes_count_offset, index_offset, vertex_offset,
                 vertices, indices);
        add_mesh("assets/models/sphere.glb", ShapeKind::Sphere, r.shapes_count_offset, index_offset, vertex_offset,
                 vertices, indices);
        add_mesh("assets/models/triangular_prism.glb", ShapeKind::TriangularPrism, r.shapes_count_offset,
                 index_offset, vertex_offset, vertices, indices);
        add_mesh("assets/models/goal.glb", ShapeKind::Goal, r.shapes_count_offset, index_offset, vertex_offset,
                 vertices, indices);
        add_mesh("assets/models/cube-player.glb", ShapeKind::BoxPlayer, r.shapes_count_offset, index_offset,
                 vertex_offset, vertices, indices);

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.ByteWidth = (u32)(sizeof(Vertex::Basic) * vertex_offset);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vinitData = {};
        vinitData.pSysMem = &vertices[0];
        r.device->CreateBuffer(&vbd, &vinitData, r.shapes_vb.GetAddressOf());

        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.ByteWidth = (u32)(sizeof(u32) * index_offset);
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA iinitData = {};
        iinitData.pSysMem = &indices[0];
        r.device->CreateBuffer(&ibd, &iinitData, r.shapes_ib.GetAddressOf());
    }

    // Creating textures for the 3D meshes.
    {
        create_texture_from_image("assets/images/floor.png", r, r.color_floor_srv);
        create_texture_from_image("assets/images/border.png", r, r.color_border_srv);
        create_texture_from_image("assets/images/scratches.jpg", r, r.specular_srv);
    }

    // Shadowmap stuff
    {
        const u32 shadowmap_width = 2048;
        const u32 shadowmap_height = 2048;

        // ComPtr<ID3D11ShaderResourceView> mDepthMapSRV;
        // ComPtr<ID3D11DepthStencilView> mDepthMapDSV;
        r.depth_map_viewport.TopLeftX = 0.0f;
        r.depth_map_viewport.TopLeftY = 0.0f;
        r.depth_map_viewport.Width = static_cast<f32>(shadowmap_width);
        r.depth_map_viewport.Height = static_cast<f32>(shadowmap_height);
        r.depth_map_viewport.MinDepth = 0.0f;
        r.depth_map_viewport.MaxDepth = 1.0f;

        // Use typeless format because the DSV is going to interpret
        // the bits as DXGI_FORMAT_D24_UNORM_S8_UINT, whereas the SRV is going to interpret
        // the bits as DXGI_FORMAT_R24_UNORM_X8_TYPELESS.
        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = shadowmap_width;
        tex_desc.Height = shadowmap_height;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        tex_desc.CPUAccessFlags = 0;
        tex_desc.MiscFlags = 0;

        ID3D11Texture2D *depth_map_texture = 0;
        r.device->CreateTexture2D(&tex_desc, 0, &depth_map_texture);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsc_desc = {};
        dsc_desc.Flags = 0;
        dsc_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsc_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsc_desc.Texture2D.MipSlice = 0;
        r.device->CreateDepthStencilView(depth_map_texture, &dsc_desc, r.depth_map_dsv.ReleaseAndGetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        r.device->CreateShaderResourceView(depth_map_texture, &srv_desc, r.depth_map_srv.ReleaseAndGetAddressOf());

        depth_map_texture->Release();
    }

    return true;
}

void rend::clear_frame(Renderer &renderer) {

    f32 clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    renderer.context->ClearRenderTargetView(renderer.render_target_view.Get(), clear_color);
    renderer.context->ClearRenderTargetView(renderer.sc_render_target_view.Get(), clear_color);
    renderer.context->ClearDepthStencilView(renderer.depth_stencil_view.Get(),
                                            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    auto render_target = renderer.render_target_view.Get();
    renderer.context->OMSetRenderTargets(1, &render_target, renderer.depth_stencil_view.Get());

#ifdef _DEBUG
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void rend::present_frame(Renderer &renderer) {
#ifdef _DEBUG
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

    // copying your render to the swapchain's back buffer
    // renderer.context->ResolveSubresource(renderer.sc_render_target_buffer.Get(), 0,
    //                                      renderer.render_target_buffer.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    renderer.swapchain->Present(1, 0);
}

void rend::clean_up() {
#ifdef _DEBUG
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
#endif
}

void rend::draw_text(Renderer &renderer, string_view text, f32 text_scale, v2 text_offset, bool centered) {
    TextString ts = {};
    ts.the_text = text;
    ts.text_scale = text_scale;
    ts.text_offset = text_offset;
    ts.centered = centered;
    renderer.text_strings.push_back(ts);
}

void rend::set_light(Renderer &renderer, DirectionalLight light) {
    renderer.light = light;
}

void rend::set_material(Renderer &renderer, Material mat) {
    renderer.mat = mat;
}

void rend::set_camera(Renderer &renderer, Camera cam) {
    renderer.camera = cam;
}

void rend::set_text_camera(Renderer &renderer, Camera cam) {
    renderer.text_camera = cam;
}

void rend::draw_shape(Renderer &renderer, Shape shape) {
    if (1.0f - shape.color.w < F32_EPSILON) {
        renderer.shapes_opaque.push_back(shape);
    } else {
        renderer.shapes_transparent.push_back(shape);
    }
}

void rend::construct_frame(Renderer &renderer) {

    auto &c = *renderer.context.Get();
    auto &res = renderer.resources;

    // writing to postfx cbuffer

    {
        i64 time_now;
        QueryPerformanceCounter((LARGE_INTEGER *)&time_now);
        f32 time_sec = (f32)((f64)time_now * renderer.seconds_per_count);

        ConstantBuffers::PostFXCBuffer postfx_data = {};

        // write res and time here
        postfx_data.res = v2((f32)renderer.client_width, (f32)renderer.client_height);
        postfx_data.time = time_sec;
        D3D11_MAPPED_SUBRESOURCE mapped_res = {};
        c.Map(res.cb_postfx.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &postfx_data, sizeof(postfx_data));
        c.Unmap(res.cb_postfx.Get(), 0);
    }

    // render background effect
    {
        {
            array cbs = {res.cb_postfx.Get()};
            c.PSSetConstantBuffers(0, (UINT)cbs.size(), cbs.data());
        }

        UINT stride = 0;
        UINT offset = 0;
        array<ID3D11Buffer *, 1> vertex_buffers = {nullptr};
        c.IASetVertexBuffers(0, (UINT)vertex_buffers.size(), vertex_buffers.data(), &stride, &offset);

        c.RSSetState(res.rs_no_cull.Get());
        c.IASetInputLayout(nullptr);
        c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        array render_targets = {renderer.render_target_view.Get()};
        c.OMSetRenderTargets((UINT)render_targets.size(), render_targets.data(), nullptr);

        c.VSSetShader(res.shaders[ShaderName::BackgroundFx].vs.Get(), nullptr, 0);
        c.PSSetShader(res.shaders[ShaderName::BackgroundFx].ps.Get(), nullptr, 0);

        c.Draw(3, 0);
    }

    XMMATRIX shadowmap_view;
    XMMATRIX shadowmap_projection;
    // use this when rendering main frame
    XMFLOAT4X4 mShadowTransform;

    // (shadow matrices)
    {
        // Setting Projection and World matrices (for shadows)
        struct BoundingSphere {
            XMFLOAT3 Center;
            f32 Radius;
        };

        BoundingSphere mSceneBounds = {};
        mSceneBounds.Center = renderer.camera_target;
        mSceneBounds.Radius = 20.0f;

        // Only the first "main" light casts a shadow.
        XMVECTOR lightDir = XMLoadFloat3(&renderer.light.Direction);
        XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
        XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        shadowmap_view = XMMatrixLookAtLH(lightPos, targetPos, up);

        // Transform bounding sphere to light space.
        XMFLOAT3 sphereCenterLS;
        XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, shadowmap_view));

        // Ortho frustum in light space encloses scene.
        float l = sphereCenterLS.x - mSceneBounds.Radius;
        float b = sphereCenterLS.y - mSceneBounds.Radius;
        float n = sphereCenterLS.z - mSceneBounds.Radius;
        float right = sphereCenterLS.x + mSceneBounds.Radius;
        float t = sphereCenterLS.y + mSceneBounds.Radius;
        float f = sphereCenterLS.z + mSceneBounds.Radius;
        shadowmap_projection = XMMatrixOrthographicOffCenterLH(l, right, b, t, n, f);

        // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
        XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);

        XMMATRIX S = shadowmap_view * shadowmap_projection * T;

        XMFLOAT4X4 mLightView;
        XMFLOAT4X4 mLightProj;

        XMStoreFloat4x4(&mLightView, shadowmap_view);
        XMStoreFloat4x4(&mLightProj, shadowmap_projection);
        XMStoreFloat4x4(&mShadowTransform, S);
    }

    const auto draw_shapes = [&c, &renderer](const span<Shape> shapes, u32 offset) {
        u32 current_shape_count = 0;
        u32 current_shape_offset = offset;
        u32 current_shape_kind = 0;

        if (shapes.size() > 0) {
            current_shape_kind = (u32)shapes[0].kind;
        }

        for (auto const &shape : shapes) {
            if (current_shape_kind != (u32)shape.kind) {

                internal_draw_shapes(renderer, c, current_shape_count, current_shape_offset, current_shape_kind);

                current_shape_offset += current_shape_count;
                current_shape_count = 0;
                current_shape_kind = (u32)shape.kind;
            }

            ++current_shape_count;
        }

        internal_draw_shapes(renderer, c, current_shape_count, current_shape_offset, current_shape_kind);
    };

    // sorting transparent boxes
    std::sort(renderer.shapes_transparent.begin(), renderer.shapes_transparent.end(),
              [renderer](const Shape &a, const Shape &b) {
                  XMVECTOR a_pos = XMLoadFloat3(&a.pos);
                  XMVECTOR b_pos = XMLoadFloat3(&b.pos);

                  auto len_a = XMVector3Length(renderer.camera.get_position_as_xmvector() - a_pos);
                  auto len_b = XMVector3Length(renderer.camera.get_position_as_xmvector() - b_pos);

                  f32 len_a_f;
                  f32 len_b_f;
                  XMStoreFloat(&len_a_f, len_a);
                  XMStoreFloat(&len_b_f, len_b);
                  return len_a_f > len_b_f;
              });

    // sorting by shape
    std::sort(renderer.shapes_opaque.begin(), renderer.shapes_opaque.end(), [](const Shape &a, const Shape &b) {
        return a.kind < b.kind;
    });

    vec<InstancedData> shapes_buffer(renderer.shapes_opaque.size() + renderer.shapes_transparent.size());

    {
        u32 buffer_i = 0;

        const auto fill_buffer_at = [&shapes_buffer, &mShadowTransform](u32 i, const Shape &shape) {
            XMMATRIX cube_scale = XMMatrixScaling(shape.scale.x, shape.scale.y, shape.scale.z);
            XMMATRIX cube_translation = XMMatrixTranslation(shape.pos.x, shape.pos.y, shape.pos.z);

            XMMATRIX world;

            if (!math::rot_is_valid(shape.rot)) {
                world = cube_scale * cube_translation;
            } else {
                XMVECTOR rot_axis = XMLoadFloat4(&shape.rot);
                XMMATRIX cube_rot = XMMatrixRotationAxis(rot_axis, shape.rot.w);
                world = cube_scale * cube_rot * cube_translation;
            }

            XMMATRIX world_inv_transpose = math::InverseTranspose(world);

            XMMATRIX shadow_mat = XMLoadFloat4x4(&mShadowTransform);

            XMStoreFloat4x4(&shapes_buffer[i].world, world);
            XMStoreFloat4x4(&shapes_buffer[i].world_inv_transpose, world_inv_transpose);
            XMStoreFloat4x4(&shapes_buffer[i].shadow_transform, world * shadow_mat);

            shapes_buffer[i].color = shape.color;
        };

        for (auto const &shape : renderer.shapes_opaque) {
            fill_buffer_at(buffer_i, shape);
            ++buffer_i;
        }

        for (auto const &shape : renderer.shapes_transparent) {
            fill_buffer_at(buffer_i, shape);
            ++buffer_i;
        }
    }

    // sending instance buffer to gpu
    {
        D3D11_MAPPED_SUBRESOURCE mapped_res = {};
        c.Map(renderer.instanced_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        if (shapes_buffer.size() > 0) {
            memcpy(mapped_res.pData, &shapes_buffer[0], sizeof(InstancedData) * shapes_buffer.size());
        }
        c.Unmap(renderer.instanced_buffer.Get(), 0);
    }

    // Rendering shadowmap
    {
        // Setting cbuffer
        ConstantBuffers::FrameCBuffer wvp_data = {};
        XMStoreFloat4x4(&wvp_data.vp, shadowmap_view * shadowmap_projection);
        D3D11_MAPPED_SUBRESOURCE mapped_res = {};
        c.Map(res.cb_frame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &wvp_data, sizeof(wvp_data));
        c.Unmap(res.cb_frame.Get(), 0);

        // render pipeline

        c.IASetInputLayout(res.il_instanced_basic.Get());
        c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        u32 stride[2] = {sizeof(Vertex::Basic), sizeof(InstancedData)};
        u32 offset[2] = {0, 0};

        ID3D11Buffer *vbs[2] = {renderer.shapes_vb.Get(), renderer.instanced_buffer.Get()};
        c.IASetVertexBuffers(0, 2, vbs, stride, offset);
        c.IASetIndexBuffer(renderer.shapes_ib.Get(), DXGI_FORMAT_R32_UINT, 0);
        c.OMSetDepthStencilState(0, 0);
        f32 blend_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        c.OMSetBlendState(0, blend_factor, 0xffffffff);

        c.RSSetViewports(1, &renderer.depth_map_viewport);

        // Set null render target because we are only going to draw to depth buffer.
        // Setting a null render target will disable color writes.
        ID3D11RenderTargetView *renderTargets[1] = {0};
        c.OMSetRenderTargets(1, renderTargets, renderer.depth_map_dsv.Get());

        c.ClearDepthStencilView(renderer.depth_map_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        c.RSSetState(res.rs_shadowmap.Get());
        // do we need special depth stencil state?

        c.VSSetShader(res.shaders[ShaderName::Shadowmap].vs.Get(), nullptr, 0);
        c.PSSetShader(nullptr, nullptr, 0);

        draw_shapes(renderer.shapes_opaque, 0);
        draw_shapes(renderer.shapes_transparent, (u32)renderer.shapes_opaque.size());

        // restoring pipeline state

        // render targets, rs state
        c.RSSetState(nullptr);
        c.RSSetViewports(1, &renderer.screen_viewport);

        renderTargets[0] = renderer.render_target_view.Get();
        c.OMSetRenderTargets(1, renderTargets, renderer.depth_stencil_view.Get());
    }

    XMMATRIX view = renderer.camera.get_view_as_xmmatrix();
    XMMATRIX proj = renderer.camera.get_proj_as_xmmatrix();

    // updating constant buffers

    // updating frame cbuffer (b0)
    {
        // get the data from app.light
        ConstantBuffers::FrameCBuffer wvp_data = {};
        XMStoreFloat4x4(&wvp_data.vp, view * proj);
        XMStoreFloat4x4(&wvp_data.view, view);
        XMStoreFloat4x4(&wvp_data.proj, proj);
        XMStoreFloat4x4(&wvp_data.view_inv, XMMatrixInverse(nullptr, view));
        XMStoreFloat4x4(&wvp_data.proj_inv, XMMatrixInverse(nullptr, proj));
        wvp_data.near_plane = renderer.camera.near_z;
        wvp_data.far_plane = renderer.camera.far_z;
        wvp_data.light = renderer.light;

        XMVECTOR new_dir = XMLoadFloat3(&wvp_data.light.Direction);
        new_dir = XMVector3Normalize(new_dir);
        XMStoreFloat3(&wvp_data.light.Direction, new_dir);
        wvp_data.eye_pos_w = renderer.camera.position;

        D3D11_MAPPED_SUBRESOURCE mapped_res = {};
        c.Map(res.cb_frame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &wvp_data, sizeof(wvp_data));
        c.Unmap(res.cb_frame.Get(), 0);
    }

    // updating object cbuffer (b1)
    {
        // get the data from app.box_material

        ConstantBuffers::ObjectCBuffer obj_data;
        obj_data.mat = renderer.mat;

        D3D11_MAPPED_SUBRESOURCE mapped_res = {};
        c.Map(res.cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
        memcpy(mapped_res.pData, &obj_data, sizeof(obj_data));
        c.Unmap(res.cb_object.Get(), 0);
    }

    // drawing boxes

    // setting rendering pipeline state
    c.IASetInputLayout(res.il_instanced_basic.Get());
    c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    {
        u32 stride[2] = {sizeof(Vertex::Basic), sizeof(InstancedData)};
        u32 offset[2] = {0, 0};

        ID3D11Buffer *vbs[2] = {renderer.shapes_vb.Get(), renderer.instanced_buffer.Get()};
        c.IASetVertexBuffers(0, 2, vbs, stride, offset);
    }
    c.IASetIndexBuffer(renderer.shapes_ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    c.RSSetState(nullptr);
    c.OMSetDepthStencilState(0, 0);
    f32 blend_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    c.OMSetBlendState(0, blend_factor, 0xffffffff);

    {
        array cbs = {res.cb_frame.Get(), res.cb_object.Get()};
        c.VSSetConstantBuffers(0, (UINT)cbs.size(), cbs.data());
        c.PSSetConstantBuffers(0, (UINT)cbs.size(), cbs.data());
    }

    {
        array samplers = {res.ss_anisotropic.Get(), res.ss_shadow.Get()};
        c.PSSetSamplers(0, (UINT)samplers.size(), samplers.data());
    }

    {
        array srvs = {renderer.color_floor_srv.Get(), renderer.specular_srv.Get(), renderer.depth_map_srv.Get()};
        c.PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
    }

    c.VSSetShader(res.shaders[ShaderName::Basic].vs.Get(), nullptr, 0);
    c.PSSetShader(res.shaders[ShaderName::Basic].ps.Get(), nullptr, 0);

    // draw opaque stuff
    draw_shapes(renderer.shapes_opaque, 0);

    // draw transparent stuff
    c.OMSetDepthStencilState(res.dss_depth_no_write.Get(), 0);
    c.OMSetBlendState(res.bs_transparent.Get(), blend_factor, 0xffffffff);
    draw_shapes(renderer.shapes_transparent, (u32)renderer.shapes_opaque.size());

    // drawing grid
    if (renderer.draw_grid) {
        c.IASetInputLayout(nullptr);
        c.IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c.RSSetState(nullptr);
        c.OMSetDepthStencilState(0, 0);
        // c.OMSetBlendState(0, blend_factor, 0xffffffff);
        c.OMSetBlendState(res.bs_transparent.Get(), blend_factor, 0xffffffff);
        {
            const u32 cbs_count = 1;
            ID3D11Buffer *cbs[cbs_count] = {res.cb_frame.Get()};
            c.VSSetConstantBuffers(0, cbs_count, cbs);
            c.PSSetConstantBuffers(0, cbs_count, cbs);
        }
        c.VSSetShader(res.shaders[ShaderName::Grid].vs.Get(), nullptr, 0);
        c.PSSetShader(res.shaders[ShaderName::Grid].ps.Get(), nullptr, 0);
        c.Draw(6, 0);
    }

    // resetting cb bindings
    {
        const u32 cbs_max_count = 5;
        ID3D11Buffer *cbs[cbs_max_count] = {0};
        c.VSSetConstantBuffers(0, cbs_max_count, cbs);
        c.PSSetConstantBuffers(0, cbs_max_count, cbs);
        array<ID3D11ShaderResourceView *, 3> srvs = {nullptr, nullptr, nullptr};
        c.PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
    }

    // render text
    for (auto const &ts : renderer.text_strings) {
        actual_render_text(renderer, ts);
    }

    // render post process
    {
        {
            array cbs = {res.cb_postfx.Get()};
            c.PSSetConstantBuffers(0, (UINT)cbs.size(), cbs.data());
        }

        UINT stride = 0;
        UINT offset = 0;
        array<ID3D11Buffer *, 1> vertex_buffers = {nullptr};
        c.IASetVertexBuffers(0, (UINT)vertex_buffers.size(), vertex_buffers.data(), &stride, &offset);

        c.RSSetState(res.rs_no_cull.Get());
        c.IASetInputLayout(nullptr);
        c.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // resolve to non-ms texture
        c.ResolveSubresource(renderer.render_target_buffer_non_ms.Get(), 0, renderer.render_target_buffer.Get(), 0,
                             DXGI_FORMAT_R8G8B8A8_UNORM);

        // set render target view to the swapchain buffer
        array render_targets = {renderer.sc_render_target_view.Get()};
        c.OMSetRenderTargets((UINT)render_targets.size(), render_targets.data(), nullptr);

        // set ps shader resources (the frame to read from)
        array srvs = {renderer.render_target_srv.Get()};
        c.PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());

        array samplers = {res.ss_basic.Get()};
        c.PSSetSamplers(0, (UINT)samplers.size(), samplers.data());

        c.VSSetShader(res.shaders[ShaderName::PostFx].vs.Get(), nullptr, 0);
        c.PSSetShader(res.shaders[ShaderName::PostFx].ps.Get(), nullptr, 0);

        c.Draw(3, 0);
    }

    // clear "draw_" state
    {
        renderer.shapes_opaque.clear();
        renderer.shapes_transparent.clear();
        renderer.text_strings.clear();
        renderer.draw_grid = false;
    }
}

void rend::draw_grid(Renderer &renderer) {
    renderer.draw_grid = true;
}

bool create_window(Ctx &ctx, HINSTANCE hInstance) {
    WNDCLASSA wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "lucy_window_class_name";

    if (!RegisterClassA(&wc)) {
        MessageBoxA(0, "RegisterClass Failed.", 0, 0);
        return false;
    }

    // Compute window rectangle dimensions based on requested client area
    // dimensions.
    RECT R = {0, 0, CLIENT_WIDTH_INITIAL, CLIENT_HEIGHT_INITIAL};
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    const string window_title = string(GAME_NAME);

    // i made it unable to be resized. the normal version would be with just this style: WS_OVERLAPPEDWINDOW
    // HWND window = CreateWindowA(wc.lpszClassName, window_title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
    //                             CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, hInstance, 0);
    HWND window = CreateWindowA(wc.lpszClassName, window_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, width, height, 0, 0, hInstance, 0);
    if (!window) {
        MessageBoxA(0, "CreateWindow Failed.", 0, 0);
        return false;
    }

    ctx.renderer->hinstance = hInstance;
    ctx.renderer->hwindow = window;
    ctx.renderer->client_width = CLIENT_WIDTH_INITIAL;
    ctx.renderer->client_height = CLIENT_HEIGHT_INITIAL;

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)&ctx);

    return true;
}

void rend::set_camera_target_pos(Renderer &renderer, v3 target) {
    renderer.camera_target = target;
}

void rend::toggle_fullscreen(Renderer &renderer) {
    toggle_fullscreen_internal(renderer.hwindow);
}

#ifdef _DEBUG

void compile_shader_to_file(string_view source_filename) {
    vec<u8> vs_b = {};
    vec<u8> ps_b = {};

    string hlsl_file = "src/shaders/";
    hlsl_file.append(source_filename);
    hlsl_file.append(".hlsl");

    compile_shader_vsps(hlsl_file, vs_b, ps_b, true);

    {
        string cso_file = "assets/shader_bin/";
        cso_file.append(source_filename);
        cso_file.append(".vs.cso");
        std::ofstream output_file(cso_file, std::ios::binary);
        output_file.write((char *)vs_b.data(), vs_b.size());
    }

    {
        string cso_file = "assets/shader_bin/";
        cso_file.append(source_filename);
        cso_file.append(".ps.cso");
        std::ofstream output_file(cso_file, std::ios::binary);
        output_file.write((char *)ps_b.data(), ps_b.size());
    }
}

void compile_all_shaders_to_files() {
    for (const auto &shader : shader_names) {
        compile_shader_to_file(shader);
    }
}

#endif