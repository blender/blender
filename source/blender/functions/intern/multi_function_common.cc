/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base_safe.h"
#include "BLI_math_vector.hh"

#include "FN_init.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_registry.hh"

#include <numeric>

namespace blender::fn::multi_function {

void register_common_functions()
{
  static constexpr auto exec_fast = build::exec_presets::AllSpanOrSingle();

  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("exp(float)", [](const float a) { return expf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "sqrt(float)", [](const float a) { return safe_sqrtf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "inverse_sqrt(float)", [](const float a) { return safe_inverse_sqrtf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "abs(float)", [](const float a) { return fabsf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "radians(float)", [](const float a) { return float(DEG2RAD(a)); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "degrees(float)", [](const float a) { return float(RAD2DEG(a)); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "sign(float)", [](const float a) { return compatible_signf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "round(float)", [](const float a) { return floorf(a + 0.5f); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "floor(float)", [](const float a) { return floorf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "ceil(float)", [](const float a) { return ceilf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "frac(float)", [](const float a) { return a - floorf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>(
        "trunc(float)", [](const float a) { return a >= 0.0f ? floorf(a) : ceilf(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("sin(float)", [](const float a) { return sinf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("cos(float)", [](const float a) { return cosf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("tan(float)", [](const float a) { return tanf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("sinh(float)", [](const float a) { return sinhf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("cosh(float)", [](const float a) { return coshf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("tanh(float)", [](const float a) { return tanhf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("asin(float)", [](const float a) { return safe_asinf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("acos(float)", [](const float a) { return safe_acosf(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float, float>("atan(float)", [](const float a) { return atanf(a); });
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float + float", [](const float a, const float b) { return a + b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float - float", [](const float a, const float b) { return a - b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float * float", [](const float a, const float b) { return a * b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float / float",
        [](const float a, const float b) { return safe_divide(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float ^ float", [](const float a, const float b) { return safe_powf(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "log(float, float)",
        [](const float a, const float b) { return safe_logf(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "min(float, float)",
        [](const float a, const float b) { return std::min(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "max(float, float)",
        [](const float a, const float b) { return std::max(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float(float < float)",
        [](const float a, const float b) { return float(a < b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float(float > float)",
        [](const float a, const float b) { return float(a > b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "float % float", [](const float a, const float b) { return safe_modf(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "floor_mod(float, float)",
        [](const float a, const float b) { return safe_floored_modf(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "snap(float, float)",
        [](const float a, const float b) { return floorf(safe_divide(a, b)) * b; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "atan2(float, float)",
        [](const float a, const float b) { return atan2f(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float, float, float>(
        "pingpong(float, float)",
        [](const float a, const float b) { return pingpongf(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float, float, float, float>(
        "float * float + float",
        [](const float a, const float b, const float c) { return a * b + c; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float, float, float, float>(
        "compare(float, float, float)",
        [](const float a, const float b, const float c) {
          return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
        },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float, float, float, float>(
        "smooth_min(float, float, float)",
        [](const float a, const float b, const float c) { return smoothminf(a, b, c); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float, float, float, float>(
        "smooth_max(float, float, float)",
        [](const float a, const float b, const float c) { return -smoothminf(-a, -b, c); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float, float, float, float>(
        "wrap(float, float, float)",
        [](const float a, const float b, const float c) { return wrapf(a, b, c); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "float3 + float3", [](const float3 &a, const float3 &b) { return a + b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "float3 - float3", [](const float3 &a, const float3 &b) { return a - b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "float3 * float3", [](const float3 &a, const float3 &b) { return a * b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "float3 / float3",
        [](const float3 &a, const float3 &b) { return math::safe_divide(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "cross_product(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::cross_high_precision(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "project(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::project(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "reflect(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::reflect(a, math::normalize(b)); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "snap(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::floor(math::safe_divide(a, b)) * b; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "float3 % float3", [](const float3 &a, const float3 &b) { return math::safe_mod(a, b); });
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "min(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::min(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>(
        "max(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::max(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float3>("float3 ^ float3", [](float3 a, float3 b) {
      return float3(safe_powf(a.x, b.x), safe_powf(a.y, b.y), safe_powf(a.z, b.z));
    });
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float>(
        "dot_product(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::dot(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float3, float>(
        "distance(float3, float3)",
        [](const float3 &a, const float3 &b) { return math::distance(a, b); },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float3, float3, float3, float3>(
        "float3 * float3 + float3",
        [](const float3 &a, const float3 &b, const float3 &c) { return a * b + c; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float3, float3, float3, float3>(
        "wrap(float3, float3, float3)", [](const float3 &a, const float3 &b, const float3 &c) {
          return float3(wrapf(a.x, b.x, c.x), wrapf(a.y, b.y, c.y), wrapf(a.z, b.z, c.z));
        });
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float3, float3, float3, float3>(
        "faceforward(float3, float3, float3)",
        [](const float3 &a, const float3 &b, const float3 &c) {
          return math::faceforward(a, b, c);
        },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI3_SO<float3, float3, float, float3>(
        "refract(float3, float3, float)", [](const float3 &a, const float3 &b, float c) {
          return math::refract(a, math::normalize(b), c);
        });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float>(
        "length(float3)", [](const float3 &a) { return math::length(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<float3, float, float3>(
        "float3 * float", [](const float3 &a, float b) { return a * b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "normalize(float3)", [](const float3 &a) { return math::normalize(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "round(float3)", [](const float3 &a) { return math::floor(a + 0.5f); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>("floor(float3)",
                                         [](const float3 &a) { return math::floor(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>("ceil(float3)",
                                         [](const float3 &a) { return math::ceil(a); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "frac(float3)", [](const float3 &a) { return math::fract(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "abs(float3)", [](const float3 &a) { return math::abs(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "sign(float3)", [](const float3 &a) { return math::sign(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "sin(float3)", [](const float3 &a) { return float3(sinf(a.x), sinf(a.y), sinf(a.z)); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "cos(float3)", [](const float3 &a) { return float3(cosf(a.x), cosf(a.y), cosf(a.z)); });
  });
  registry::add_new_cb([] {
    return build::SI1_SO<float3, float3>(
        "tan(float3)", [](const float3 &a) { return float3(tanf(a.x), tanf(a.y), tanf(a.z)); });
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int + int", [](int a, int b) { return a + b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int - int", [](int a, int b) { return a - b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int * int", [](int a, int b) { return a * b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int / int", [](int a, int b) { return math::safe_divide(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "floor(int, int)",
        [](int a, int b) { return (b != 0) ? divide_floor_i(a, b) : 0; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "divide_ceil(int, int)",
        [](int a, int b) { return (b != 0) ? -divide_floor_i(a, -b) : 0; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "divide_round(int, int)",
        [](int a, int b) {
          /* Derived from `divide_round_i` but fixed to be safe and handle negative inputs. */
          const int c = math::abs(b);
          return (a >= 0) ? math::safe_divide((2 * a + c), (2 * c)) * math::sign(b) :
                            -math::safe_divide((2 * -a + c), (2 * c)) * math::sign(b);
        },
        exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int ^ int", [](int a, int b) { return math::pow(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI3_SO<int, int, int, int>(
        "int * int + int", [](int a, int b, int c) { return a * b + c; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "mod_periodic(int, int)",
        [](int a, int b) { return b != 0 ? math::mod_periodic(a, b) : 0; },
        exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int % int", [](int a, int b) { return b != 0 ? a % b : 0; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI1_SO<int, int>("abs(int)", [](int a) { return math::abs(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI1_SO<int, int>(
        "sign(int)", [](int a) { return math::sign(a); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "min(int, int)", [](int a, int b) { return math::min(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "max(int, int)", [](int a, int b) { return math::max(a, b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>("gcd(int, int)",
                                            [](int a, int b) { return std::gcd(a, b); });
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>("lcm(int, int)",
                                            [](int a, int b) { return std::lcm(a, b); });
  });
  registry::add_new_cb(
      [] { return mf::build::SI1_SO<int, int>("-int", [](int a) { return -a; }, exec_fast); });
}

}  // namespace blender::fn::multi_function
