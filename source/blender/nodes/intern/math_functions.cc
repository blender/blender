/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_math_functions.hh"

#include "FN_multi_function_registry.hh"

namespace blender::nodes {

static const mf::MultiFunction *get_base_multi_function(const bNode &node)
{
  const int mode = node.custom1;
  const FloatMathOperationInfo *info = get_float_math_operation_info(mode);
  if (!info) {
    return nullptr;
  }
  return &fn::multi_function::registry::lookup(info->multi_function_name);
}

class ClampWrapperFunction : public mf::MultiFunction {
 private:
  const mf::MultiFunction &fn_;

 public:
  ClampWrapperFunction(const mf::MultiFunction &fn) : fn_(fn)
  {
    this->set_signature(&fn.signature());
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context context) const override
  {
    fn_.call(mask, params, context);

    /* Assumes the output parameter is the last one. */
    const int output_param_index = this->param_amount() - 1;
    /* This has actually been initialized in the call above. */
    MutableSpan<float> results = params.uninitialized_single_output<float>(output_param_index);

    mask.foreach_index_optimized<int>([&](const int i) {
      float &value = results[i];
      CLAMP(value, 0.0f, 1.0f);
    });
  }
};

void node_math_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *base_function = get_base_multi_function(builder.node());

  const bool clamp_output = builder.node().custom2 != 0;
  if (clamp_output) {
    builder.construct_and_set_matching_fn<ClampWrapperFunction>(*base_function);
  }
  else {
    builder.set_matching_fn(base_function);
  }
}

const FloatMathOperationInfo *get_float_math_operation_info(const int operation)
{
#define RETURN_OPERATION_INFO(title_case_name, shader_name, multi_function_name) \
  { \
    static const FloatMathOperationInfo info{title_case_name, shader_name, multi_function_name}; \
    return &info; \
  } \
  ((void)0)

  switch (operation) {
    case NODE_MATH_ADD:
      RETURN_OPERATION_INFO("Add", "math_add", "float + float"_ustr);
    case NODE_MATH_SUBTRACT:
      RETURN_OPERATION_INFO("Subtract", "math_subtract", "float - float"_ustr);
    case NODE_MATH_MULTIPLY:
      RETURN_OPERATION_INFO("Multiply", "math_multiply", "float * float"_ustr);
    case NODE_MATH_DIVIDE:
      RETURN_OPERATION_INFO("Divide", "math_divide", "float / float"_ustr);
    case NODE_MATH_SINE:
      RETURN_OPERATION_INFO("Sine", "math_sine", "sin(float)"_ustr);
    case NODE_MATH_COSINE:
      RETURN_OPERATION_INFO("Cosine", "math_cosine", "cos(float)"_ustr);
    case NODE_MATH_TANGENT:
      RETURN_OPERATION_INFO("Tangent", "math_tangent", "tan(float)"_ustr);
    case NODE_MATH_ARCSINE:
      RETURN_OPERATION_INFO("Arc Sine", "math_arcsine", "asin(float)"_ustr);
    case NODE_MATH_ARCCOSINE:
      RETURN_OPERATION_INFO("Arc Cosine", "math_arccosine", "acos(float)"_ustr);
    case NODE_MATH_ARCTANGENT:
      RETURN_OPERATION_INFO("Arc Tangent", "math_arctangent", "atan(float)"_ustr);
    case NODE_MATH_POWER:
      RETURN_OPERATION_INFO("Power", "math_power", "float ^ float"_ustr);
    case NODE_MATH_LOGARITHM:
      RETURN_OPERATION_INFO("Logarithm", "math_logarithm", "log(float, float)"_ustr);
    case NODE_MATH_MINIMUM:
      RETURN_OPERATION_INFO("Minimum", "math_minimum", "min(float, float)"_ustr);
    case NODE_MATH_MAXIMUM:
      RETURN_OPERATION_INFO("Maximum", "math_maximum", "max(float, float)"_ustr);
    case NODE_MATH_ROUND:
      RETURN_OPERATION_INFO("Round", "math_round", "round(float)"_ustr);
    case NODE_MATH_LESS_THAN:
      RETURN_OPERATION_INFO("Less Than", "math_less_than", "float(float < float)"_ustr);
    case NODE_MATH_GREATER_THAN:
      RETURN_OPERATION_INFO("Greater Than", "math_greater_than", "float(float > float)"_ustr);
    case NODE_MATH_MODULO:
      RETURN_OPERATION_INFO("Modulo", "math_modulo", "float % float"_ustr);
    case NODE_MATH_FLOORED_MODULO:
      RETURN_OPERATION_INFO(
          "Floored Modulo", "math_floored_modulo", "floor_mod(float, float)"_ustr);
    case NODE_MATH_ABSOLUTE:
      RETURN_OPERATION_INFO("Absolute", "math_absolute", "abs(float)"_ustr);
    case NODE_MATH_ARCTAN2:
      RETURN_OPERATION_INFO("Arc Tangent 2", "math_arctan2", "atan2(float, float)"_ustr);
    case NODE_MATH_FLOOR:
      RETURN_OPERATION_INFO("Floor", "math_floor", "floor(float)"_ustr);
    case NODE_MATH_CEIL:
      RETURN_OPERATION_INFO("Ceil", "math_ceil", "ceil(float)"_ustr);
    case NODE_MATH_FRACTION:
      RETURN_OPERATION_INFO("Fraction", "math_fraction", "frac(float)"_ustr);
    case NODE_MATH_SQRT:
      RETURN_OPERATION_INFO("Sqrt", "math_sqrt", "sqrt(float)"_ustr);
    case NODE_MATH_INV_SQRT:
      RETURN_OPERATION_INFO("Inverse Sqrt", "math_inversesqrt", "inverse_sqrt(float)"_ustr);
    case NODE_MATH_SIGN:
      RETURN_OPERATION_INFO("Sign", "math_sign", "sign(float)"_ustr);
    case NODE_MATH_EXPONENT:
      RETURN_OPERATION_INFO("Exponent", "math_exponent", "exp(float)"_ustr);
    case NODE_MATH_RADIANS:
      RETURN_OPERATION_INFO("Radians", "math_radians", "radians(float)"_ustr);
    case NODE_MATH_DEGREES:
      RETURN_OPERATION_INFO("Degrees", "math_degrees", "degrees(float)"_ustr);
    case NODE_MATH_SINH:
      RETURN_OPERATION_INFO("Hyperbolic Sine", "math_sinh", "sinh(float)"_ustr);
    case NODE_MATH_COSH:
      RETURN_OPERATION_INFO("Hyperbolic Cosine", "math_cosh", "cosh(float)"_ustr);
    case NODE_MATH_TANH:
      RETURN_OPERATION_INFO("Hyperbolic Tangent", "math_tanh", "tanh(float)"_ustr);
    case NODE_MATH_TRUNC:
      RETURN_OPERATION_INFO("Truncate", "math_trunc", "trunc(float)"_ustr);
    case NODE_MATH_SNAP:
      RETURN_OPERATION_INFO("Snap", "math_snap", "snap(float, float)"_ustr);
    case NODE_MATH_WRAP:
      RETURN_OPERATION_INFO("Wrap", "math_wrap", "wrap(float, float, float)"_ustr);
    case NODE_MATH_COMPARE:
      RETURN_OPERATION_INFO("Compare", "math_compare", "compare(float, float, float)"_ustr);
    case NODE_MATH_MULTIPLY_ADD:
      RETURN_OPERATION_INFO("Multiply Add", "math_multiply_add", "float * float + float"_ustr);
    case NODE_MATH_PINGPONG:
      RETURN_OPERATION_INFO("Ping Pong", "math_pingpong", "pingpong(float, float)"_ustr);
    case NODE_MATH_SMOOTH_MIN:
      RETURN_OPERATION_INFO(
          "Smooth Min", "math_smoothmin", "smooth_min(float, float, float)"_ustr);
    case NODE_MATH_SMOOTH_MAX:
      RETURN_OPERATION_INFO(
          "Smooth Max", "math_smoothmax", "smooth_max(float, float, float)"_ustr);
  }

#undef RETURN_OPERATION_INFO

  return nullptr;
}

const FloatMathOperationInfo *get_float_compare_operation_info(const int operation)
{

#define RETURN_OPERATION_INFO(title_case_name, shader_name) \
  { \
    static const FloatMathOperationInfo info{title_case_name, shader_name}; \
    return &info; \
  } \
  ((void)0)

  switch (operation) {
    case NODE_COMPARE_LESS_THAN:
      RETURN_OPERATION_INFO("Less Than", "math_less_than");
    case NODE_COMPARE_LESS_EQUAL:
      RETURN_OPERATION_INFO("Less Than or Equal", "math_less_equal");
    case NODE_COMPARE_GREATER_THAN:
      RETURN_OPERATION_INFO("Greater Than", "math_greater_than");
    case NODE_COMPARE_GREATER_EQUAL:
      RETURN_OPERATION_INFO("Greater Than or Equal", "math_greater_equal");
    case NODE_COMPARE_EQUAL:
      RETURN_OPERATION_INFO("Equal", "math_equal");
    case NODE_COMPARE_NOT_EQUAL:
      RETURN_OPERATION_INFO("Not Equal", "math_not_equal");
  }

#undef RETURN_OPERATION_INFO

  return nullptr;
}

const FloatMathOperationInfo *get_float3_math_operation_info(const int operation)
{

#define RETURN_OPERATION_INFO(title_case_name, shader_name, multi_function_name) \
  { \
    static const FloatMathOperationInfo info{title_case_name, shader_name, multi_function_name}; \
    return &info; \
  } \
  ((void)0)

  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
      RETURN_OPERATION_INFO("Add", "vector_math_add", "float3 + float3"_ustr);
    case NODE_VECTOR_MATH_SUBTRACT:
      RETURN_OPERATION_INFO("Subtract", "vector_math_subtract", "float3 - float3"_ustr);
    case NODE_VECTOR_MATH_MULTIPLY:
      RETURN_OPERATION_INFO("Multiply", "vector_math_multiply", "float3 * float3"_ustr);
    case NODE_VECTOR_MATH_DIVIDE:
      RETURN_OPERATION_INFO("Divide", "vector_math_divide", "float3 / float3"_ustr);
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      RETURN_OPERATION_INFO(
          "Cross Product", "vector_math_cross", "cross_product(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_PROJECT:
      RETURN_OPERATION_INFO("Project", "vector_math_project", "project(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_REFLECT:
      RETURN_OPERATION_INFO("Reflect", "vector_math_reflect", "reflect(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      RETURN_OPERATION_INFO("Dot Product", "vector_math_dot", "dot_product(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_DISTANCE:
      RETURN_OPERATION_INFO("Distance", "vector_math_distance", "distance(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_LENGTH:
      RETURN_OPERATION_INFO("Length", "vector_math_length", "length(float3)"_ustr);
    case NODE_VECTOR_MATH_SCALE:
      RETURN_OPERATION_INFO("Scale", "vector_math_scale", "float3 * float"_ustr);
    case NODE_VECTOR_MATH_NORMALIZE:
      RETURN_OPERATION_INFO("Normalize", "vector_math_normalize", "normalize(float3)"_ustr);
    case NODE_VECTOR_MATH_SNAP:
      RETURN_OPERATION_INFO("Snap", "vector_math_snap", "snap(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_ROUND:
      RETURN_OPERATION_INFO("Round", "vector_math_round", "round(float3)"_ustr);
    case NODE_VECTOR_MATH_FLOOR:
      RETURN_OPERATION_INFO("Floor", "vector_math_floor", "floor(float3)"_ustr);
    case NODE_VECTOR_MATH_CEIL:
      RETURN_OPERATION_INFO("Ceiling", "vector_math_ceil", "ceil(float3)"_ustr);
    case NODE_VECTOR_MATH_MODULO:
      RETURN_OPERATION_INFO("Modulo", "vector_math_modulo", "float3 % float3"_ustr);
    case NODE_VECTOR_MATH_FRACTION:
      RETURN_OPERATION_INFO("Fraction", "vector_math_fraction", "frac(float3)"_ustr);
    case NODE_VECTOR_MATH_ABSOLUTE:
      RETURN_OPERATION_INFO("Absolute", "vector_math_absolute", "abs(float3)"_ustr);
    case NODE_VECTOR_MATH_MINIMUM:
      RETURN_OPERATION_INFO("Minimum", "vector_math_minimum", "min(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_MAXIMUM:
      RETURN_OPERATION_INFO("Maximum", "vector_math_maximum", "max(float3, float3)"_ustr);
    case NODE_VECTOR_MATH_WRAP:
      RETURN_OPERATION_INFO("Wrap", "vector_math_wrap", "wrap(float3, float3, float3)"_ustr);
    case NODE_VECTOR_MATH_SINE:
      RETURN_OPERATION_INFO("Sine", "vector_math_sine", "sin(float3)"_ustr);
    case NODE_VECTOR_MATH_COSINE:
      RETURN_OPERATION_INFO("Cosine", "vector_math_cosine", "cos(float3)"_ustr);
    case NODE_VECTOR_MATH_TANGENT:
      RETURN_OPERATION_INFO("Tangent", "vector_math_tangent", "tan(float3)"_ustr);
    case NODE_VECTOR_MATH_REFRACT:
      RETURN_OPERATION_INFO(
          "Refract", "vector_math_refract", "refract(float3, float3, float)"_ustr);
    case NODE_VECTOR_MATH_FACEFORWARD:
      RETURN_OPERATION_INFO(
          "Faceforward", "vector_math_faceforward", "faceforward(float3, float3, float3)"_ustr);
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      RETURN_OPERATION_INFO(
          "Multiply Add", "vector_math_multiply_add", "float3 * float3 + float3"_ustr);
    case NODE_VECTOR_MATH_POWER:
      RETURN_OPERATION_INFO("Power", "vector_math_power", "float3 ^ float3"_ustr);
    case NODE_VECTOR_MATH_SIGN:
      RETURN_OPERATION_INFO("Sign", "vector_math_sign", "sign(float3)"_ustr);
  }

#undef RETURN_OPERATION_INFO

  return nullptr;
}

}  // namespace blender::nodes
