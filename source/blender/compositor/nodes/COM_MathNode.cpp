/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 */

#include "COM_MathNode.h"
#include "COM_MathBaseOperation.h"
#include "COM_ExecutionSystem.h"

void MathNode::convertToOperations(NodeConverter &converter, const CompositorContext &/*context*/) const
{
	MathBaseOperation *operation = NULL;

	switch (this->getbNode()->custom1) {
		case NODE_MATH_ADD:
			operation = new MathAddOperation();
			break;
		case NODE_MATH_SUB:
			operation = new MathSubtractOperation();
			break;
		case NODE_MATH_MUL:
			operation = new MathMultiplyOperation();
			break;
		case NODE_MATH_DIVIDE:
			operation = new MathDivideOperation();
			break;
		case NODE_MATH_SIN:
			operation = new MathSineOperation();
			break;
		case NODE_MATH_COS:
			operation = new MathCosineOperation();
			break;
		case NODE_MATH_TAN:
			operation = new MathTangentOperation();
			break;
		case NODE_MATH_ASIN:
			operation = new MathArcSineOperation();
			break;
		case NODE_MATH_ACOS:
			operation = new MathArcCosineOperation();
			break;
		case NODE_MATH_ATAN:
			operation = new MathArcTangentOperation();
			break;
		case NODE_MATH_POW:
			operation = new MathPowerOperation();
			break;
		case NODE_MATH_LOG:
			operation = new MathLogarithmOperation();
			break;
		case NODE_MATH_MIN:
			operation = new MathMinimumOperation();
			break;
		case NODE_MATH_MAX:
			operation = new MathMaximumOperation();
			break;
		case NODE_MATH_ROUND:
			operation = new MathRoundOperation();
			break;
		case NODE_MATH_LESS:
			operation = new MathLessThanOperation();
			break;
		case NODE_MATH_GREATER:
			operation = new MathGreaterThanOperation();
			break;
		case NODE_MATH_MOD:
			operation = new MathModuloOperation();
			break;
		case NODE_MATH_ABS:
			operation = new MathAbsoluteOperation();
			break;
		case NODE_MATH_ATAN2:
			operation = new MathArcTan2Operation();
			break;
		case NODE_MATH_FLOOR:
			operation = new MathFloorOperation();
			break;
		case NODE_MATH_CEIL:
			operation = new MathCeilOperation();
			break;
		case NODE_MATH_FRACT:
			operation = new MathFractOperation();
			break;
		case NODE_MATH_SQRT:
			operation = new MathSqrtOperation();
			break;
	}

	if (operation) {
		bool useClamp = getbNode()->custom2;
		operation->setUseClamp(useClamp);
		converter.addOperation(operation);

		converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
		converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
	}
}
