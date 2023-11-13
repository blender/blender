/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_math_functions.hh"

namespace blender::nodes {

const FloatMathOperationInfo *get_float_math_operation_info(const int operation)
{

#define RETURN_OPERATION_INFO(title_case_name, shader_name) \
  { \
    static const FloatMathOperationInfo info{title_case_name, shader_name}; \
    return &info; \
  } \
  ((void)0)

  switch (operation) {
    case NODE_MATH_ADD:
      RETURN_OPERATION_INFO("Add", "math_add");
    case NODE_MATH_SUBTRACT:
      RETURN_OPERATION_INFO("Subtract", "math_subtract");
    case NODE_MATH_MULTIPLY:
      RETURN_OPERATION_INFO("Multiply", "math_multiply");
    case NODE_MATH_DIVIDE:
      RETURN_OPERATION_INFO("Divide", "math_divide");
    case NODE_MATH_SINE:
      RETURN_OPERATION_INFO("Sine", "math_sine");
    case NODE_MATH_COSINE:
      RETURN_OPERATION_INFO("Cosine", "math_cosine");
    case NODE_MATH_TANGENT:
      RETURN_OPERATION_INFO("Tangent", "math_tangent");
    case NODE_MATH_ARCSINE:
      RETURN_OPERATION_INFO("Arc Sine", "math_arcsine");
    case NODE_MATH_ARCCOSINE:
      RETURN_OPERATION_INFO("Arc Cosine", "math_arccosine");
    case NODE_MATH_ARCTANGENT:
      RETURN_OPERATION_INFO("Arc Tangent", "math_arctangent");
    case NODE_MATH_POWER:
      RETURN_OPERATION_INFO("Power", "math_power");
    case NODE_MATH_LOGARITHM:
      RETURN_OPERATION_INFO("Logarithm", "math_logarithm");
    case NODE_MATH_MINIMUM:
      RETURN_OPERATION_INFO("Minimum", "math_minimum");
    case NODE_MATH_MAXIMUM:
      RETURN_OPERATION_INFO("Maximum", "math_maximum");
    case NODE_MATH_ROUND:
      RETURN_OPERATION_INFO("Round", "math_round");
    case NODE_MATH_LESS_THAN:
      RETURN_OPERATION_INFO("Less Than", "math_less_than");
    case NODE_MATH_GREATER_THAN:
      RETURN_OPERATION_INFO("Greater Than", "math_greater_than");
    case NODE_MATH_MODULO:
      RETURN_OPERATION_INFO("Modulo", "math_modulo");
    case NODE_MATH_FLOORED_MODULO:
      RETURN_OPERATION_INFO("Floored Modulo", "math_floored_modulo");
    case NODE_MATH_ABSOLUTE:
      RETURN_OPERATION_INFO("Absolute", "math_absolute");
    case NODE_MATH_ARCTAN2:
      RETURN_OPERATION_INFO("Arc Tangent 2", "math_arctan2");
    case NODE_MATH_FLOOR:
      RETURN_OPERATION_INFO("Floor", "math_floor");
    case NODE_MATH_CEIL:
      RETURN_OPERATION_INFO("Ceil", "math_ceil");
    case NODE_MATH_FRACTION:
      RETURN_OPERATION_INFO("Fraction", "math_fraction");
    case NODE_MATH_SQRT:
      RETURN_OPERATION_INFO("Sqrt", "math_sqrt");
    case NODE_MATH_INV_SQRT:
      RETURN_OPERATION_INFO("Inverse Sqrt", "math_inversesqrt");
    case NODE_MATH_SIGN:
      RETURN_OPERATION_INFO("Sign", "math_sign");
    case NODE_MATH_EXPONENT:
      RETURN_OPERATION_INFO("Exponent", "math_exponent");
    case NODE_MATH_RADIANS:
      RETURN_OPERATION_INFO("Radians", "math_radians");
    case NODE_MATH_DEGREES:
      RETURN_OPERATION_INFO("Degrees", "math_degrees");
    case NODE_MATH_SINH:
      RETURN_OPERATION_INFO("Hyperbolic Sine", "math_sinh");
    case NODE_MATH_COSH:
      RETURN_OPERATION_INFO("Hyperbolic Cosine", "math_cosh");
    case NODE_MATH_TANH:
      RETURN_OPERATION_INFO("Hyperbolic Tangent", "math_tanh");
    case NODE_MATH_TRUNC:
      RETURN_OPERATION_INFO("Truncate", "math_trunc");
    case NODE_MATH_SNAP:
      RETURN_OPERATION_INFO("Snap", "math_snap");
    case NODE_MATH_WRAP:
      RETURN_OPERATION_INFO("Wrap", "math_wrap");
    case NODE_MATH_COMPARE:
      RETURN_OPERATION_INFO("Compare", "math_compare");
    case NODE_MATH_MULTIPLY_ADD:
      RETURN_OPERATION_INFO("Multiply Add", "math_multiply_add");
    case NODE_MATH_PINGPONG:
      RETURN_OPERATION_INFO("Ping Pong", "math_pingpong");
    case NODE_MATH_SMOOTH_MIN:
      RETURN_OPERATION_INFO("Smooth Min", "math_smoothmin");
    case NODE_MATH_SMOOTH_MAX:
      RETURN_OPERATION_INFO("Smooth Max", "math_smoothmax");
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

#define RETURN_OPERATION_INFO(title_case_name, shader_name) \
  { \
    static const FloatMathOperationInfo info{title_case_name, shader_name}; \
    return &info; \
  } \
  ((void)0)

  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
      RETURN_OPERATION_INFO("Add", "vector_math_add");
    case NODE_VECTOR_MATH_SUBTRACT:
      RETURN_OPERATION_INFO("Subtract", "vector_math_subtract");
    case NODE_VECTOR_MATH_MULTIPLY:
      RETURN_OPERATION_INFO("Multiply", "vector_math_multiply");
    case NODE_VECTOR_MATH_DIVIDE:
      RETURN_OPERATION_INFO("Divide", "vector_math_divide");
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      RETURN_OPERATION_INFO("Cross Product", "vector_math_cross");
    case NODE_VECTOR_MATH_PROJECT:
      RETURN_OPERATION_INFO("Project", "vector_math_project");
    case NODE_VECTOR_MATH_REFLECT:
      RETURN_OPERATION_INFO("Reflect", "vector_math_reflect");
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      RETURN_OPERATION_INFO("Dot Product", "vector_math_dot");
    case NODE_VECTOR_MATH_DISTANCE:
      RETURN_OPERATION_INFO("Distance", "vector_math_distance");
    case NODE_VECTOR_MATH_LENGTH:
      RETURN_OPERATION_INFO("Length", "vector_math_length");
    case NODE_VECTOR_MATH_SCALE:
      RETURN_OPERATION_INFO("Scale", "vector_math_scale");
    case NODE_VECTOR_MATH_NORMALIZE:
      RETURN_OPERATION_INFO("Normalize", "vector_math_normalize");
    case NODE_VECTOR_MATH_SNAP:
      RETURN_OPERATION_INFO("Snap", "vector_math_snap");
    case NODE_VECTOR_MATH_FLOOR:
      RETURN_OPERATION_INFO("Floor", "vector_math_floor");
    case NODE_VECTOR_MATH_CEIL:
      RETURN_OPERATION_INFO("Ceiling", "vector_math_ceil");
    case NODE_VECTOR_MATH_MODULO:
      RETURN_OPERATION_INFO("Modulo", "vector_math_modulo");
    case NODE_VECTOR_MATH_FRACTION:
      RETURN_OPERATION_INFO("Fraction", "vector_math_fraction");
    case NODE_VECTOR_MATH_ABSOLUTE:
      RETURN_OPERATION_INFO("Absolute", "vector_math_absolute");
    case NODE_VECTOR_MATH_MINIMUM:
      RETURN_OPERATION_INFO("Minimum", "vector_math_minimum");
    case NODE_VECTOR_MATH_MAXIMUM:
      RETURN_OPERATION_INFO("Maximum", "vector_math_maximum");
    case NODE_VECTOR_MATH_WRAP:
      RETURN_OPERATION_INFO("Wrap", "vector_math_wrap");
    case NODE_VECTOR_MATH_SINE:
      RETURN_OPERATION_INFO("Sine", "vector_math_sine");
    case NODE_VECTOR_MATH_COSINE:
      RETURN_OPERATION_INFO("Cosine", "vector_math_cosine");
    case NODE_VECTOR_MATH_TANGENT:
      RETURN_OPERATION_INFO("Tangent", "vector_math_tangent");
    case NODE_VECTOR_MATH_REFRACT:
      RETURN_OPERATION_INFO("Refract", "vector_math_refract");
    case NODE_VECTOR_MATH_FACEFORWARD:
      RETURN_OPERATION_INFO("Faceforward", "vector_math_faceforward");
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      RETURN_OPERATION_INFO("Multiply Add", "vector_math_multiply_add");
  }

#undef RETURN_OPERATION_INFO

  return nullptr;
}

}  // namespace blender::nodes
