#pragma once

#include <stdint.h>
#include <DirectXMath.h>

// my types
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using v2 = DirectX::XMFLOAT2;
using v3 = DirectX::XMFLOAT3;
using v4 = DirectX::XMFLOAT4;
using m4 = DirectX::XMFLOAT4X4;

#include <limits>
#include <span>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <format>

using std::array;
using std::format;
using std::span;
using std::string;
using std::string_view;
using std::vector;

template <typename T>
using vec = std::vector<T>;

#define arrsize(arr) (sizeof(arr) / sizeof(arr[0]))

const inline constexpr f32 F32_EPSILON = std::numeric_limits<f32>::epsilon();

using namespace std::literals;