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

/**
 * An multi-function for the powf operation that more optimally handles simple and
 * common cases like raising to the power of 2.
 */
class PowFunction : public MultiFunction {
 private:
  static inline const MultiFunction *pow_generic = nullptr;
  static inline const MultiFunction *pow_2 = nullptr;
  static inline const MultiFunction *pow_3 = nullptr;

 public:
  PowFunction()
  {
    static Signature signature = []() {
      pow_2 = &registry::lookup("float ** 2"_ustr);
      pow_3 = &registry::lookup("float ** 3"_ustr);
      static auto pow_generic_fn = build::SI2_SO<float, float, float>(
          "pow generic",
          [](const float a, const float b) { return safe_powf(a, b); },
          build::exec_presets::Materialized());
      pow_generic = &pow_generic_fn;

      Signature signature;
      SignatureBuilder builder("float ** float", signature);
      builder.single_input<float>("Base");
      builder.single_input<float>("Exponent");
      builder.single_output<float>("Result");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, Params params, Context context) const override
  {
    /* Use GVArray here to avoid unnecessary conversions to typed virtual arrays. */
    const GVArray &base = params.readonly_single_input(0, "Base");
    const GVArray &exponent = params.readonly_single_input(1, "Exponent");
    MutableSpan<float> result = params.uninitialized_single_output<float>(2, "Result");

    if (exponent.is_single()) {
      float exponent_single;
      exponent.get_internal_single(&exponent_single);
      const int exponent_int = int(exponent_single);
      /* Handle some exponents without invoking the general powf function. */
      if (float(exponent_int) == exponent_single) {
        switch (exponent_int) {
          case 0: {
            index_mask::masked_fill(result, 1.0f, mask);
            return;
          }
          case 1: {
            base.materialize_to_uninitialized(mask, result.data());
            return;
          }
          case 2: {
            ParamsBuilder sub_params{*pow_2, &mask};
            sub_params.add_readonly_single_input(base);
            sub_params.add_uninitialized_single_output(result);
            pow_2->call(mask, sub_params, context);
            return;
          }
          case 3: {
            ParamsBuilder sub_params{*pow_3, &mask};
            sub_params.add_readonly_single_input(base);
            sub_params.add_uninitialized_single_output(result);
            pow_3->call(mask, sub_params, context);
            return;
          }
          default: {
            break;
          }
        }
      }
    }
    pow_generic->call(mask, params, context);
  }
};

class DivideFunction : public MultiFunction {
 private:
  static inline const MultiFunction *multiply = nullptr;
  static inline const MultiFunction *divide_generic = nullptr;

 public:
  DivideFunction()
  {
    static Signature signature = []() {
      multiply = &registry::lookup("float * float"_ustr);
      static auto divide_generic_fn = build::SI2_SO<float, float, float>(
          "float / float",
          [](float a, float b) { return safe_divide(a, b); },
          build::exec_presets::AllSpanOrSingle());
      divide_generic = &divide_generic_fn;

      Signature signature;
      SignatureBuilder builder("float / float", signature);
      builder.single_input<float>("A");
      builder.single_input<float>("B");
      builder.single_output<float>("Result");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context context) const override
  {
    const GVArray &a = params.readonly_single_input(0, "A");
    const GVArray &b = params.readonly_single_input(1, "B");
    MutableSpan<float> result = params.uninitialized_single_output<float>(2, "Result");

    if (b.is_single()) {
      float divisor;
      b.get_internal_single(&divisor);
      if (divisor == 0.0f) {
        /* We define the output to be 0 for division by zero. Same as #safe_divide. */
        index_mask::masked_fill(result, 0.0f, mask);
        return;
      }
      if (divisor == 1.0f) {
        /* If the divisor is 1 the result is the dividend. */
        a.materialize_to_uninitialized(mask, result.data());
        return;
      }
      if (is_inverse_exact(divisor)) {
        /* Use multiplication by the inverse which is more efficient than division. */
        const float inverse = 1.0f / divisor;
        ParamsBuilder sub_params{*multiply, &mask};
        sub_params.add_readonly_single_input(a);
        sub_params.add_readonly_single_input_value(inverse);
        sub_params.add_uninitialized_single_output(result);
        multiply->call(mask, sub_params, context);
        return;
      }
    }
    if (a.is_single()) {
      float dividend;
      a.get_internal_single(&dividend);
      if (dividend == 0.0f) {
        /* If the dividend is zero the result is always zero regardless of the divisor. */
        index_mask::masked_fill(result, 0.0f, mask);
        return;
      }
    }
    /* General case. */
    divide_generic->call(mask, params, context);
  }

  static bool is_inverse_exact(float x)
  {
    BLI_assert(x != 0.0f);
    x = fabsf(x);
    int exp;
    /* Check that x is a power of two. */
    const float fraction = frexpf(x, &exp);
    return fraction == 0.5f;
  }
};

static void register_common_functions_impl()
{
  static constexpr auto exec_fast = build::exec_presets::AllSpanOrSingle();

  registry::add_new_cb([]() {
    return build::SI1_SO<float, float>(
        "float ** 2", [](const float a) { return a * a; }, exec_fast);
  });
  registry::add_new_cb([]() {
    return build::SI1_SO<float, float>(
        "float ** 3", [](const float a) { return a * a * a; }, exec_fast);
  });
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
  registry::add_new_cb([] { return DivideFunction(); });
  registry::add_new_cb([] { return PowFunction(); });
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
    return build::SI2_SO<float3, float3, float3>("float3 ** float3", [](float3 a, float3 b) {
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
        "int ** int", [](int a, int b) { return math::pow(a, b); }, exec_fast);
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
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "bool && bool", [](bool a, bool b) { return a && b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "bool || bool", [](bool a, bool b) { return a || b; }, exec_fast);
  });
  registry::add_new_cb(
      [] { return mf::build::SI1_SO<bool, bool>("!bool", [](bool a) { return !a; }, exec_fast); });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "!(bool && bool)", [](bool a, bool b) { return !(a && b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "!(bool || bool)", [](bool a, bool b) { return !(a || b); }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "bool == bool", [](bool a, bool b) { return a == b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "bool != bool", [](bool a, bool b) { return a != b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "!bool || bool", [](bool a, bool b) { return !a || b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<bool, bool, bool>(
        "bool && !bool", [](bool a, bool b) { return a && !b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int & int", [](int a, int b) { return a & b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int | int", [](int a, int b) { return a | b; }, exec_fast);
  });
  registry::add_new_cb([] {
    return mf::build::SI2_SO<int, int, int>(
        "int ^ int", [](int a, int b) { return a ^ b; }, exec_fast);
  });
  registry::add_new_cb(
      [] { return build::SI1_SO<int, int>("~int", [](int a) { return ~a; }, exec_fast); });
  registry::add_new_cb([] {
    return build::SI2_SO<int, int, int>(
        "shift(int, int)",
        [](int a, int b) {
          const uint32_t value = a;
          const int shift = math::clamp(b, -32, 32);
          const uint64_t wide_value = uint64_t(value) << 16;
          const uint64_t wide_result = shift > 0 ? wide_value << shift : wide_value >> -shift;
          return uint32_t(wide_result >> 16);
        },
        exec_fast);
  });
  registry::add_new_cb([] {
    return build::SI2_SO<int, int, int>(
        "rotate(int, int)",
        [](int a, int b) {
          const uint32_t value = a;
          const int shift = math::mod_periodic(b, 32);
          const uint64_t wide_value = uint64_t(value) | (uint64_t(value) << 32);
          const uint64_t double_result = (wide_value << shift);
          return uint32_t((double_result | (double_result >> 32)) & ((uint64_t(1) << 33) - 1));
        },
        exec_fast);
  });
}

void register_common_functions()
{
  /* Make sure the functions are only registered once even if called multiple times. */
  [[maybe_unused]] static bool registered = []() {
    register_common_functions_impl();
    return true;
  }();
}

}  // namespace blender::fn::multi_function
