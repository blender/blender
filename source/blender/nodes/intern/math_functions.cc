/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

}  // namespace blender::nodes
