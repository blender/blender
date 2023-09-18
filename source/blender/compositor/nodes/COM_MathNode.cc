/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MathNode.h"
#include "COM_MathBaseOperation.h"

namespace blender::compositor {

void MathNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  MathBaseOperation *operation = nullptr;

  switch (this->get_bnode()->custom1) {
    case NODE_MATH_ADD:
      operation = new MathAddOperation();
      break;
    case NODE_MATH_SUBTRACT:
      operation = new MathSubtractOperation();
      break;
    case NODE_MATH_MULTIPLY:
      operation = new MathMultiplyOperation();
      break;
    case NODE_MATH_DIVIDE:
      operation = new MathDivideOperation();
      break;
    case NODE_MATH_SINE:
      operation = new MathSineOperation();
      break;
    case NODE_MATH_COSINE:
      operation = new MathCosineOperation();
      break;
    case NODE_MATH_TANGENT:
      operation = new MathTangentOperation();
      break;
    case NODE_MATH_ARCSINE:
      operation = new MathArcSineOperation();
      break;
    case NODE_MATH_ARCCOSINE:
      operation = new MathArcCosineOperation();
      break;
    case NODE_MATH_ARCTANGENT:
      operation = new MathArcTangentOperation();
      break;
    case NODE_MATH_SINH:
      operation = new MathHyperbolicSineOperation();
      break;
    case NODE_MATH_COSH:
      operation = new MathHyperbolicCosineOperation();
      break;
    case NODE_MATH_TANH:
      operation = new MathHyperbolicTangentOperation();
      break;
    case NODE_MATH_POWER:
      operation = new MathPowerOperation();
      break;
    case NODE_MATH_LOGARITHM:
      operation = new MathLogarithmOperation();
      break;
    case NODE_MATH_MINIMUM:
      operation = new MathMinimumOperation();
      break;
    case NODE_MATH_MAXIMUM:
      operation = new MathMaximumOperation();
      break;
    case NODE_MATH_ROUND:
      operation = new MathRoundOperation();
      break;
    case NODE_MATH_LESS_THAN:
      operation = new MathLessThanOperation();
      break;
    case NODE_MATH_GREATER_THAN:
      operation = new MathGreaterThanOperation();
      break;
    case NODE_MATH_MODULO:
      operation = new MathModuloOperation();
      break;
    case NODE_MATH_FLOORED_MODULO:
      operation = new MathFlooredModuloOperation();
      break;
    case NODE_MATH_ABSOLUTE:
      operation = new MathAbsoluteOperation();
      break;
    case NODE_MATH_RADIANS:
      operation = new MathRadiansOperation();
      break;
    case NODE_MATH_DEGREES:
      operation = new MathDegreesOperation();
      break;
    case NODE_MATH_ARCTAN2:
      operation = new MathArcTan2Operation();
      break;
    case NODE_MATH_FLOOR:
      operation = new MathFloorOperation();
      break;
    case NODE_MATH_CEIL:
      operation = new MathCeilOperation();
      break;
    case NODE_MATH_FRACTION:
      operation = new MathFractOperation();
      break;
    case NODE_MATH_SQRT:
      operation = new MathSqrtOperation();
      break;
    case NODE_MATH_INV_SQRT:
      operation = new MathInverseSqrtOperation();
      break;
    case NODE_MATH_SIGN:
      operation = new MathSignOperation();
      break;
    case NODE_MATH_EXPONENT:
      operation = new MathExponentOperation();
      break;
    case NODE_MATH_TRUNC:
      operation = new MathTruncOperation();
      break;
    case NODE_MATH_SNAP:
      operation = new MathSnapOperation();
      break;
    case NODE_MATH_WRAP:
      operation = new MathWrapOperation();
      break;
    case NODE_MATH_PINGPONG:
      operation = new MathPingpongOperation();
      break;
    case NODE_MATH_COMPARE:
      operation = new MathCompareOperation();
      break;
    case NODE_MATH_MULTIPLY_ADD:
      operation = new MathMultiplyAddOperation();
      break;
    case NODE_MATH_SMOOTH_MIN:
      operation = new MathSmoothMinOperation();
      break;
    case NODE_MATH_SMOOTH_MAX:
      operation = new MathSmoothMaxOperation();
      break;
  }

  if (operation) {
    bool use_clamp = get_bnode()->custom2;
    operation->set_use_clamp(use_clamp);
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
  }
}

}  // namespace blender::compositor
