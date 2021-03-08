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
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_MathNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_MathBaseOperation.h"

void MathNode::convertToOperations(NodeConverter &converter,
                                   const CompositorContext & /*context*/) const
{
  MathBaseOperation *operation = nullptr;

  switch (this->getbNode()->custom1) {
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
    bool useClamp = getbNode()->custom2;
    operation->setUseClamp(useClamp);
    converter.addOperation(operation);

    converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
    converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
    converter.mapInputSocket(getInputSocket(2), operation->getInputSocket(2));
    converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
  }
}
