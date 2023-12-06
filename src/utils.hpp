#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <intrin.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "lucytypes.hpp"

using DirectX::CXMMATRIX;
using DirectX::XMMATRIX;

void log(const char *format, ...);
vec<u8> load_file(string_view filename, bool strip_windows_endline = false);

#define arrlen(x) (sizeof(x) / sizeof(x[0]))

#ifdef _DEBUG
#define lassert(expr)                                                                                              \
    if (!(expr))                                                                                                   \
        __debugbreak();
#else
#define lassert(expr)                                                                                              \
    if (!(expr)) {                                                                                                 \
        MessageBoxA(0, "Some assertion failed. Critical error.", 0, 0);                                            \
        exit(1);                                                                                                   \
    }
#endif

#ifdef _DEBUG
#define lassert_s(expr, str)                                                                                       \
    if (!(expr))                                                                                                   \
        __debugbreak();
#else
#define lassert_s(expr, str)                                                                                       \
    if (!(expr)) {                                                                                                 \
        MessageBoxA(0, str, 0, 0);                                                                                 \
        exit(1);                                                                                                   \
    }
#endif

namespace math {

const inline constexpr f32 Infinity = FLT_MAX;
const inline constexpr f32 Pi = 3.1415926535f;
const inline constexpr f32 Tau = Pi * 2.0f;

// Returns the polar angle of the point (x,y) in [0, 2*PI).
float AngleFromXY(float x, float y);

template <typename T>
T Min(const T &a, const T &b) {
    return a < b ? a : b;
}

template <typename T>
T Max(const T &a, const T &b) {
    return a > b ? a : b;
}

// Returns random float in [0, 1).
f32 randf();
f32 randf(float a, float b);
i32 randi(i32 a, i32 b);
f32 clampf(f32 value, f32 min, f32 max);
XMMATRIX InverseTranspose(CXMMATRIX M);

// sets the forth to 0
v4 v3tov4(v3 v);
// it's like v.xyz in hlsl
v3 v4tov3(v4 v);
bool v4_is_zero(v4 v);
// checks if the first 3 components are not (0, 0, 0)
bool rot_is_valid(v4 v);
v3 v3_mul(v3 a, v3 b);
v4 v4_mul(v4 a, v4 b);
// 1, 1, 1
v3 v3_one();
// 1, 1, 1, 1
v4 v4_one();
// 1, 0, 0, 1
v4 v4_red();
// 0, 1, 0, 1
v4 v4_green();
// 0, 0, 1, 1
v4 v4_blue();

// makes it positive and % Tau
f32 make_angle(f32 angle);

// radians to turns
f32 rtot(f32 angle_rad);

// returns the negative version of ang_a if it is the closest to ang_b. otherwise positive.
f32 make_closest(f32 ang_a, f32 ang_b);
} // namespace math

// parsing utilities
namespace parse {

struct Parser {
    string *b;
    u32 index;
};

struct ParseResult {
    bool succeeded;
    u32 i_next;
};

// if found, returns true and sets index_next to the index + str.size()
ParseResult is_next(Parser p, string_view str);
// if found, returns true and sets index_next to the first char of the first
// occurrence of str
ParseResult advance_until(Parser p, string_view str);

} // namespace parse