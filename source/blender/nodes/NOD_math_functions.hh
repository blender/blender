/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "BLI_math_base_safe.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_string_ref.hh"

#include "FN_multi_function_builder.hh"

namespace blender::nodes {

struct FloatMathOperationInfo {
  StringRefNull title_case_name;
  StringRefNull shader_name;

  FloatMathOperationInfo() = delete;
  FloatMathOperationInfo(StringRefNull title_case_name, StringRefNull shader_name)
      : title_case_name(title_case_name), shader_name(shader_name)
  {
  }
};

const FloatMathOperationInfo *get_float_math_operation_info(int operation);
const FloatMathOperationInfo *get_float3_math_operation_info(int operation);
const FloatMathOperationInfo *get_float_compare_operation_info(int operation);

/**
 * This calls the `callback` with two arguments:
 * 1. The math function that takes a float as input and outputs a new float.
 * 2. A #FloatMathOperationInfo struct reference.
 * Returns true when the callback has been called, otherwise false.
 *
 * The math function that is passed to the callback is actually a lambda function that is different
 * for every operation. Therefore, if the callback is templated on the math function, it will get
 * instantiated for every operation separately. This has two benefits:
 * - The compiler can optimize the callback for every operation separately.
 * - A static variable declared in the callback will be generated for every operation separately.
 *
 * If separate instantiations are not desired, the callback can also take a function pointer with
 * the following signature as input instead: float (*math_function)(float a).
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();
  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_EXPONENT:
      return dispatch(exec_preset_slow, [](float a) { return expf(a); });
    case NODE_MATH_SQRT:
      return dispatch(exec_preset_fast, [](float a) { return safe_sqrtf(a); });
    case NODE_MATH_INV_SQRT:
      return dispatch(exec_preset_fast, [](float a) { return safe_inverse_sqrtf(a); });
    case NODE_MATH_ABSOLUTE:
      return dispatch(exec_preset_fast, [](float a) { return fabs(a); });
    case NODE_MATH_RADIANS:
      return dispatch(exec_preset_fast, [](float a) { return (float)DEG2RAD(a); });
    case NODE_MATH_DEGREES:
      return dispatch(exec_preset_fast, [](float a) { return (float)RAD2DEG(a); });
    case NODE_MATH_SIGN:
      return dispatch(exec_preset_fast, [](float a) { return compatible_signf(a); });
    case NODE_MATH_ROUND:
      return dispatch(exec_preset_fast, [](float a) { return floorf(a + 0.5f); });
    case NODE_MATH_FLOOR:
      return dispatch(exec_preset_fast, [](float a) { return floorf(a); });
    case NODE_MATH_CEIL:
      return dispatch(exec_preset_fast, [](float a) { return ceilf(a); });
    case NODE_MATH_FRACTION:
      return dispatch(exec_preset_fast, [](float a) { return a - floorf(a); });
    case NODE_MATH_TRUNC:
      return dispatch(exec_preset_fast, [](float a) { return a >= 0.0f ? floorf(a) : ceilf(a); });
    case NODE_MATH_SINE:
      return dispatch(exec_preset_slow, [](float a) { return sinf(a); });
    case NODE_MATH_COSINE:
      return dispatch(exec_preset_slow, [](float a) { return cosf(a); });
    case NODE_MATH_TANGENT:
      return dispatch(exec_preset_slow, [](float a) { return tanf(a); });
    case NODE_MATH_SINH:
      return dispatch(exec_preset_slow, [](float a) { return sinhf(a); });
    case NODE_MATH_COSH:
      return dispatch(exec_preset_slow, [](float a) { return coshf(a); });
    case NODE_MATH_TANH:
      return dispatch(exec_preset_slow, [](float a) { return tanhf(a); });
    case NODE_MATH_ARCSINE:
      return dispatch(exec_preset_slow, [](float a) { return safe_asinf(a); });
    case NODE_MATH_ARCCOSINE:
      return dispatch(exec_preset_slow, [](float a) { return safe_acosf(a); });
    case NODE_MATH_ARCTANGENT:
      return dispatch(exec_preset_slow, [](float a) { return atanf(a); });
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();
  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_ADD:
      return dispatch(exec_preset_fast, [](float a, float b) { return a + b; });
    case NODE_MATH_SUBTRACT:
      return dispatch(exec_preset_fast, [](float a, float b) { return a - b; });
    case NODE_MATH_MULTIPLY:
      return dispatch(exec_preset_fast, [](float a, float b) { return a * b; });
    case NODE_MATH_DIVIDE:
      return dispatch(exec_preset_fast, [](float a, float b) { return safe_divide(a, b); });
    case NODE_MATH_POWER:
      return dispatch(exec_preset_slow, [](float a, float b) { return safe_powf(a, b); });
    case NODE_MATH_LOGARITHM:
      return dispatch(exec_preset_slow, [](float a, float b) { return safe_logf(a, b); });
    case NODE_MATH_MINIMUM:
      return dispatch(exec_preset_fast, [](float a, float b) { return std::min(a, b); });
    case NODE_MATH_MAXIMUM:
      return dispatch(exec_preset_fast, [](float a, float b) { return std::max(a, b); });
    case NODE_MATH_LESS_THAN:
      return dispatch(exec_preset_fast, [](float a, float b) { return (float)(a < b); });
    case NODE_MATH_GREATER_THAN:
      return dispatch(exec_preset_fast, [](float a, float b) { return (float)(a > b); });
    case NODE_MATH_MODULO:
      return dispatch(exec_preset_fast, [](float a, float b) { return safe_modf(a, b); });
    case NODE_MATH_SNAP:
      return dispatch(exec_preset_fast,
                      [](float a, float b) { return floorf(safe_divide(a, b)) * b; });
    case NODE_MATH_ARCTAN2:
      return dispatch(exec_preset_slow, [](float a, float b) { return atan2f(a, b); });
    case NODE_MATH_PINGPONG:
      return dispatch(exec_preset_fast, [](float a, float b) { return pingpongf(a, b); });
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl_fl_fl_to_fl(const int operation, Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  /* This is just an utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_MATH_MULTIPLY_ADD:
      return dispatch(mf::build::exec_presets::AllSpanOrSingle(),
                      [](float a, float b, float c) { return a * b + c; });
    case NODE_MATH_COMPARE:
      return dispatch(mf::build::exec_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) -> float {
                        return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
                      });
    case NODE_MATH_SMOOTH_MIN:
      return dispatch(mf::build::exec_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) { return smoothminf(a, b, c); });
    case NODE_MATH_SMOOTH_MAX:
      return dispatch(mf::build::exec_presets::SomeSpanOrSingle<0, 1>(),
                      [](float a, float b, float c) { return -smoothminf(-a, -b, c); });
    case NODE_MATH_WRAP:
      return dispatch(mf::build::exec_presets::SomeSpanOrSingle<0>(),
                      [](float a, float b, float c) { return wrapf(a, b, c); });
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_to_fl3(const NodeVectorMathOperation operation,
                                                   Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();
  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return a + b; });
    case NODE_VECTOR_MATH_SUBTRACT:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return a - b; });
    case NODE_VECTOR_MATH_MULTIPLY:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return a * b; });
    case NODE_VECTOR_MATH_DIVIDE:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return safe_divide(a, b); });
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      return dispatch(exec_preset_fast,
                      [](float3 a, float3 b) { return cross_high_precision(a, b); });
    case NODE_VECTOR_MATH_PROJECT:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return project(a, b); });
    case NODE_VECTOR_MATH_REFLECT:
      return dispatch(exec_preset_fast,
                      [](float3 a, float3 b) { return reflect(a, normalize(b)); });
    case NODE_VECTOR_MATH_SNAP:
      return dispatch(exec_preset_fast,
                      [](float3 a, float3 b) { return floor(safe_divide(a, b)) * b; });
    case NODE_VECTOR_MATH_MODULO:
      return dispatch(exec_preset_slow, [](float3 a, float3 b) { return mod(a, b); });
    case NODE_VECTOR_MATH_MINIMUM:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return min(a, b); });
    case NODE_VECTOR_MATH_MAXIMUM:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return max(a, b); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_to_fl(const NodeVectorMathOperation operation,
                                                  Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return dot(a, b); });
    case NODE_VECTOR_MATH_DISTANCE:
      return dispatch(exec_preset_fast, [](float3 a, float3 b) { return distance(a, b); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_fl3_to_fl3(const NodeVectorMathOperation operation,
                                                       Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();
  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      return dispatch(exec_preset_fast, [](float3 a, float3 b, float3 c) { return a * b + c; });
    case NODE_VECTOR_MATH_WRAP:
      return dispatch(exec_preset_slow, [](float3 a, float3 b, float3 c) {
        return float3(wrapf(a.x, b.x, c.x), wrapf(a.y, b.y, c.y), wrapf(a.z, b.z, c.z));
      });
    case NODE_VECTOR_MATH_FACEFORWARD:
      return dispatch(exec_preset_fast,
                      [](float3 a, float3 b, float3 c) { return faceforward(a, b, c); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl3_fl_to_fl3(const NodeVectorMathOperation operation,
                                                      Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_REFRACT:
      return dispatch(exec_preset_slow,
                      [](float3 a, float3 b, float c) { return refract(a, normalize(b), c); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_to_fl(const NodeVectorMathOperation operation,
                                              Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_LENGTH:
      return dispatch(exec_preset_fast, [](float3 in) { return length(in); });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_fl_to_fl3(const NodeVectorMathOperation operation,
                                                  Callback &&callback)
{
  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_SCALE:
      return dispatch(exec_preset_fast, [](float3 a, float b) { return a * b; });
    default:
      return false;
  }
  return false;
}

/**
 * This is similar to try_dispatch_float_math_fl_to_fl, just with a different callback signature.
 */
template<typename Callback>
inline bool try_dispatch_float_math_fl3_to_fl3(const NodeVectorMathOperation operation,
                                               Callback &&callback)
{
  using namespace blender::math;

  const FloatMathOperationInfo *info = get_float3_math_operation_info(operation);
  if (info == nullptr) {
    return false;
  }

  static auto exec_preset_fast = mf::build::exec_presets::AllSpanOrSingle();
  static auto exec_preset_slow = mf::build::exec_presets::Materialized();

  /* This is just a utility function to keep the individual cases smaller. */
  auto dispatch = [&](auto exec_preset, auto math_function) -> bool {
    callback(exec_preset, math_function, *info);
    return true;
  };

  switch (operation) {
    case NODE_VECTOR_MATH_NORMALIZE:
      /* Should be safe. */
      return dispatch(exec_preset_fast, [](float3 in) { return normalize(in); });
    case NODE_VECTOR_MATH_FLOOR:
      return dispatch(exec_preset_fast, [](float3 in) { return floor(in); });
    case NODE_VECTOR_MATH_CEIL:
      return dispatch(exec_preset_fast, [](float3 in) { return ceil(in); });
    case NODE_VECTOR_MATH_FRACTION:
      return dispatch(exec_preset_fast, [](float3 in) { return fract(in); });
    case NODE_VECTOR_MATH_ABSOLUTE:
      return dispatch(exec_preset_fast, [](float3 in) { return abs(in); });
    case NODE_VECTOR_MATH_SINE:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(sinf(in.x), sinf(in.y), sinf(in.z)); });
    case NODE_VECTOR_MATH_COSINE:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(cosf(in.x), cosf(in.y), cosf(in.z)); });
    case NODE_VECTOR_MATH_TANGENT:
      return dispatch(exec_preset_slow,
                      [](float3 in) { return float3(tanf(in.x), tanf(in.y), tanf(in.z)); });
    default:
      return false;
  }
  return false;
}

}  // namespace blender::nodes
