#include "utils.hpp"

#include <cstdlib>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <fstream>
#include <windows.h>

using parse::ParseResult;

// beautiful and flexible non-error prone code
// it mallocs if it has to print something bigger than buf_len
void log(const char *format, ...) {
    va_list argp;
    const size_t buf_len = 100;
    char buf[buf_len];
    char *target = buf;
    va_start(argp, format);
    i32 len = _vscprintf(format, argp) + 2;
    bool did_alloc = false;
    if (len > (i32)buf_len) {
        target = (char *)malloc(sizeof(char) * len);
        did_alloc = true;
    }
    i32 written = vsprintf_s(target, len, format, argp);
    va_end(argp);

    target[written] = '\n';
    target[written + 1] = '\0';
    OutputDebugStringA(target);

    if (did_alloc) {
        free(target);
    }
}

vec<u8> load_file(string_view filename, bool strip_windows_endline) {

    std::ifstream file(string(filename), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        lassert(false);
    }

    std::streampos fileSize = file.tellg();
    vec<u8> the_data(fileSize);

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(the_data.data()), fileSize);
    file.close();

    if (strip_windows_endline) {
        auto new_end = std::remove(the_data.begin(), the_data.end(), '\r');
        the_data.erase(new_end, the_data.end());
    }

    return the_data;
}

// math stuff

float math::AngleFromXY(float x, float y) {
    float theta = 0.0f;

    // Quadrant I or IV
    if (x >= 0.0f) {
        // If x = 0, then atanf(y/x) = +pi/2 if y > 0
        //                atanf(y/x) = -pi/2 if y < 0
        theta = atanf(y / x); // in [-pi/2, +pi/2]

        if (theta < 0.0f)
            theta += 2.0f * Pi; // in [0, 2*pi).
    }

    // Quadrant II or III
    else
        theta = atanf(y / x) + Pi; // in [0, 2*pi).

    return theta;
}

f32 math::clampf(f32 value, f32 min, f32 max) {
    if (value > max)
        return max;

    if (value < min)
        return min;

    return value;
}

DirectX::XMMATRIX math::InverseTranspose(DirectX::CXMMATRIX M) {
    // Inverse-transpose is just applied to normals.  So zero out
    // translation row so that it doesn't get into our inverse-transpose
    // calculation--we don't want the inverse-transpose of the translation.
    DirectX::XMMATRIX A = M;
    A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
    return DirectX::XMMatrixTranspose(XMMatrixInverse(&det, A));
}

ParseResult parse::is_next(Parser p, string_view str) {

    ParseResult r = {};

    string sub_str = (*p.b).substr(p.index, str.size());

    if (sub_str != str) {
        r.succeeded = false;
        return r;
    }

    r.succeeded = true;
    r.i_next = p.index + (i32)str.size();

    return r;
}

ParseResult parse::advance_until(Parser p, string_view str) {

    ParseResult r = {};

    size_t res = (*p.b).find(str, p.index);

    if (res == string::npos) {
        r.succeeded = false;
        return r;
    }

    r.i_next = (i32)res;
    r.succeeded = true;

    return r;
}

v4 math::v3tov4(v3 v) {
    return v4(v.x, v.y, v.z, 0.0f);
}

v3 math::v4tov3(v4 v) {
    return v3(v.x, v.y, v.z);
}

f32 math::randf() {
    return (float)(rand()) / (float)RAND_MAX;
}

f32 math::randf(float a, float b) {
    return a + randf() * (b - a);
}

i32 math::randi(i32 a, i32 b) {
    return (i32)(((double)rand() / RAND_MAX) * (b - a) + a);
}

bool math::v4_is_zero(v4 v) {
    return v.x == 0.f && v.y == 0.f && v.z == 0.f && v.w == 0.f;
}

// checks if the first 3 components are not (0, 0, 0)
bool math::rot_is_valid(v4 v) {
    return !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
}
// 1, 1, 1
v3 math::v3_one() {
    return v3(1.0f, 1.0f, 1.0f);
}
// 1, 1, 1, 1
v4 math::v4_one() {
    return v4(1.0f, 1.0f, 1.0f, 1.0f);
}
// 1, 0, 0, 1
v4 math::v4_red() {
    return v4(1.0f, 0.0f, 0.0f, 1.0f);
}
// 0, 1, 0, 1
v4 math::v4_green() {
    return v4(0.0f, 1.0f, 0.0f, 1.0f);
}
// 0, 0, 1, 1
v4 math::v4_blue() {
    return v4(0.0f, 0.0f, 1.0f, 1.0f);
}

v3 math::v3_mul(v3 a, v3 b) {
    return v3(a.x * b.x, a.y * b.y, a.z * b.z);
}

v4 math::v4_mul(v4 a, v4 b) {
    return v4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

f32 math::make_angle(f32 angle) {
    while (angle < 0.f) {
        angle = Tau + angle;
    }

    while (angle > Tau) {
        angle -= Tau;
    }

    return angle;
}

f32 math::make_closest(f32 ang_a, f32 ang_b) {
    if (abs(ang_a - ang_b) > Pi) {
        return ang_a - Tau;
    }

    return ang_a;
}

f32 math::rtot(f32 angle_rad) {
    return angle_rad / Tau;
}