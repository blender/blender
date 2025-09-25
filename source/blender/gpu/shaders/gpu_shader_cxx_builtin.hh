/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include "gpu_shader_cxx_matrix.hh"
#include "gpu_shader_cxx_vector.hh"

/* Some compilers complain about lack of return values. Keep it short. */
#define RET \
  { \
    return {}; \
  }

/* -------------------------------------------------------------------- */
/** \name Builtin Functions
 * \{ */

template<typename T, int D> VecBase<bool, D> greaterThan(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<bool, D> lessThan(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<bool, D> lessThanEqual(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<bool, D> greaterThanEqual(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<bool, D> equal(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<bool, D> notEqual(VecOp<T, D>, VecOp<T, D>) RET;
template<int D> bool any(VecOp<bool, D>) RET;
template<int D> bool all(VecOp<bool, D>) RET;
/* `not` is a C++ keyword that aliases the `!` operator. Simply overload it. */
template<int D> VecBase<bool, D> operator!(VecOp<bool, D>) RET;

template<int D> VecBase<int, D> bitCount(VecOp<int, D>) RET;
template<int D> VecBase<int, D> bitCount(VecOp<uint, D>) RET;
template<int D> VecBase<int, D> bitfieldExtract(VecOp<int, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldExtract(VecOp<uint, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldInsert(VecOp<int, D>, VecOp<int, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldInsert(VecOp<uint, D>, VecOp<uint, D>, int, int) RET;
template<int D> VecBase<int, D> bitfieldReverse(VecOp<int, D>) RET;
template<int D> VecBase<int, D> bitfieldReverse(VecOp<uint, D>) RET;
int bitCount(int) RET;
int bitCount(uint) RET;
int bitfieldExtract(int) RET;
int bitfieldExtract(uint) RET;
int bitfieldInsert(int) RET;
int bitfieldInsert(uint) RET;
int bitfieldReverse(int) RET;
int bitfieldReverse(uint) RET;

template<int D> VecBase<int, D> findLSB(VecOp<int, D>) RET;
template<int D> VecBase<int, D> findLSB(VecOp<uint, D>) RET;
template<int D> VecBase<int, D> findMSB(VecOp<int, D>) RET;
template<int D> VecBase<int, D> findMSB(VecOp<uint, D>) RET;
int findLSB(int) RET;
int findLSB(uint) RET;
int findMSB(int) RET;
int findMSB(uint) RET;

/* Math Functions. */

/* NOTE: Declared inside a namespace and exposed behind macros to prevent
 * errors on VS2019 due to `corecrt_math` conflicting functions. */
namespace glsl {
template<typename T> constexpr T abs(T) RET;
/* TODO(fclem): These should be restricted to floats. */
template<typename T> constexpr T ceil(T) RET;
template<typename T> constexpr T exp(T) RET;
template<typename T> constexpr T exp2(T) RET;
template<typename T> constexpr T floor(T) RET;
template<typename T> T fma(T, T, T) RET;
float fma(float, float, float) RET;
template<typename T> T frexp(T, T) RET;
bool isinf(float) RET;
template<int D> VecBase<bool, D> isinf(VecOp<float, D>) RET;
bool isnan(float) RET;
template<int D> VecBase<bool, D> isnan(VecOp<float, D>) RET;
template<typename T> constexpr T log(T) RET;
template<typename T> constexpr T log2(T) RET;
template<typename T> T modf(T, T);
template<typename T, typename U> constexpr T pow(T, U) RET;
template<typename T> constexpr T round(T) RET;
template<typename T> constexpr T sqrt(T) RET;
template<typename T> constexpr T trunc(T) RET;
template<typename T, typename U> T ldexp(T, U) RET;

template<typename T> constexpr T acos(T) RET;
template<typename T> T acosh(T) RET;
template<typename T> constexpr T asin(T) RET;
template<typename T> T asinh(T) RET;
template<typename T> T atan(T, T) RET;
template<typename T> T atan(T) RET;
template<typename T> T atanh(T) RET;
template<typename T> constexpr T cos(T) RET;
template<typename T> T cosh(T) RET;
template<typename T> constexpr T sin(T) RET;
template<typename T> T sinh(T) RET;
template<typename T> T tan(T) RET;
template<typename T> T tanh(T) RET;
}  // namespace glsl

#define abs glsl::abs
#define ceil glsl::ceil
#define exp glsl::exp
#define exp2 glsl::exp2
#define floor glsl::floor
#define fma glsl::fma
#define frexp glsl::frexp
#define isinf glsl::isinf
#define isnan glsl::isnan
#define log glsl::log
#define log2 glsl::log2
#define modf glsl::modf
#define pow glsl::pow
#define round glsl::round
#define sqrt glsl::sqrt
#define trunc glsl::trunc
#define ldexp glsl::ldexp
#define acos glsl::acos
#define acosh glsl::acosh
#define asin glsl::asin
#define asinh glsl::asinh
#define atan glsl::atan
#define atanh glsl::atanh
#define cos glsl::cos
#define cosh glsl::cosh
#define sin glsl::sin
#define sinh glsl::sinh
#define tan glsl::tan
#define tanh glsl::tanh

template<typename T> constexpr T max(T, T) RET;
template<typename T> constexpr T min(T, T) RET;
template<typename T> constexpr T sign(T) RET;
template<typename T, typename U> constexpr T clamp(T, U, U) RET;
template<typename T> constexpr T clamp(T, float, float) RET;
template<typename T, typename U> constexpr T max(T, U) RET;
template<typename T, typename U> constexpr T min(T, U) RET;
/* TODO(fclem): These should be restricted to floats. */
template<typename T> T fract(T) RET;
template<typename T> constexpr T inversesqrt(T) RET;
constexpr float mod(float, float) RET;
template<int D> VecBase<float, D> constexpr mod(VecOp<float, D>, float) RET;
template<int D> VecBase<float, D> constexpr mod(VecOp<float, D>, VecOp<float, D>) RET;
template<typename T> T smoothstep(T, T, T) RET;
float step(float, float) RET;
template<int D> VecBase<float, D> step(VecOp<float, D>, VecOp<float, D>) RET;
template<int D> VecBase<float, D> step(float, VecOp<float, D>) RET;
float smoothstep(float, float, float) RET;
template<int D> VecBase<float, D> smoothstep(float, float, VecOp<float, D>) RET;

template<typename T> constexpr T degrees(T) RET;
template<typename T> constexpr T radians(T) RET;

/* Declared explicitly to avoid type errors. */
float mix(float, float, float) RET;
template<int D> VecBase<float, D> mix(VecOp<float, D>, VecOp<float, D>, float) RET;
template<int D> VecBase<float, D> mix(VecOp<float, D>, VecOp<float, D>, VecOp<float, D>) RET;
template<typename T, int D> VecBase<T, D> mix(VecOp<T, D>, VecOp<T, D>, VecOp<bool, D>) RET;

#define select(A, B, C) mix(A, B, C)

VecBase<float, 3> cross(VecOp<float, 3>, VecOp<float, 3>) RET;
template<int D> float dot(VecOp<float, D>, VecOp<float, D>) RET;
template<int D> float distance(VecOp<float, D>, VecOp<float, D>) RET;
template<int D> float length(VecOp<float, D>) RET;
template<int D> VecBase<float, D> normalize(VecOp<float, D>) RET;

template<int D> VecBase<int, D> floatBitsToInt(VecOp<float, D>) RET;
template<int D> VecBase<uint, D> floatBitsToUint(VecOp<float, D>) RET;
template<int D> VecBase<float, D> intBitsToFloat(VecOp<int, D>) RET;
template<int D> VecBase<float, D> uintBitsToFloat(VecOp<uint, D>) RET;
int floatBitsToInt(float) RET;
uint floatBitsToUint(float) RET;
float intBitsToFloat(int) RET;
float uintBitsToFloat(uint) RET;

/* Derivative functions. */
template<typename T> T gpu_dfdx(T) RET;
template<typename T> T gpu_dfdy(T) RET;
template<typename T> T gpu_fwidth(T) RET;

/* Discards the output of the current fragment shader invocation and halts its execution. */
void gpu_discard_fragment() {}

/* Geometric functions. */
template<typename T, int D> VecBase<T, D> faceforward(VecOp<T, D>, VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<T, D> reflect(VecOp<T, D>, VecOp<T, D>) RET;
template<typename T, int D> VecBase<T, D> refract(VecOp<T, D>, VecOp<T, D>, float) RET;

/* Atomic operations. */
int atomicAdd(int &, int) RET;
int atomicAnd(int &, int) RET;
int atomicOr(int &, int) RET;
int atomicXor(int &, int) RET;
int atomicMin(int &, int) RET;
int atomicMax(int &, int) RET;
int atomicExchange(int &, int) RET;
int atomicCompSwap(int &, int, int) RET;
uint atomicAdd(uint &, uint) RET;
uint atomicAnd(uint &, uint) RET;
uint atomicOr(uint &, uint) RET;
uint atomicXor(uint &, uint) RET;
uint atomicMin(uint &, uint) RET;
uint atomicMax(uint &, uint) RET;
uint atomicExchange(uint &, uint) RET;
uint atomicCompSwap(uint &, uint, uint) RET;

/* Packing functions. */
uint packHalf2x16(float2) RET;
uint packUnorm2x16(float2) RET;
uint packSnorm2x16(float2) RET;
uint packUnorm4x8(float4) RET;
uint packSnorm4x8(float4) RET;
float2 unpackHalf2x16(uint) RET;
float2 unpackUnorm2x16(uint) RET;
float2 unpackSnorm2x16(uint) RET;
float4 unpackUnorm4x8(uint) RET;
float4 unpackSnorm4x8(uint) RET;

/* Matrices functions. */
template<int C, int R> float determinant(MatBase<C, R>) RET;
template<int C, int R> MatBase<C, R> inverse(MatBase<C, R>) RET;
template<int C, int R> MatBase<R, C> transpose(MatBase<C, R>) RET;

namespace gl_ComputeShader {
void barrier() {}
void memoryBarrier() {}
void memoryBarrierShared() {}
void memoryBarrierImage() {}
void memoryBarrierBuffer() {}
void groupMemoryBarrier() {}
}  // namespace gl_ComputeShader

/** \} */

#undef RET
