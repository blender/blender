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

void MathNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	MathBaseOperation *operation = NULL;
	
	switch (this->getbNode()->custom1) {
		case 0: /* Add */
			operation = new MathAddOperation();
			break;
		case 1: /* Subtract */
			operation = new MathSubtractOperation();
			break;
		case 2: /* Multiply */
			operation = new MathMultiplyOperation();
			break;
		case 3: /* Divide */
			operation = new MathDivideOperation();
			break;
		case 4: /* Sine */
			operation = new MathSineOperation();
			break;
		case 5: /* Cosine */
			operation = new MathCosineOperation();
			break;
		case 6: /* Tangent */
			operation = new MathTangentOperation();
			break;
		case 7: /* Arc-Sine */
			operation = new MathArcSineOperation();
			break;
		case 8: /* Arc-Cosine */
			operation = new MathArcCosineOperation();
			break;
		case 9: /* Arc-Tangent */
			operation = new MathArcTangentOperation();
			break;
		case 10: /* Power */
			operation = new MathPowerOperation();
			break;
		case 11: /* Logarithm */
			operation = new MathLogarithmOperation();
			break;
		case 12: /* Minimum */
			operation = new MathMinimumOperation();
			break;
		case 13: /* Maximum */
			operation = new MathMaximumOperation();
			break;
		case 14: /* Round */
			operation = new MathRoundOperation();
			break;
		case 15: /* Less Than */
			operation = new MathLessThanOperation();
			break;
		case 16: /* Greater Than */
			operation = new MathGreaterThanOperation();
			break;
		case 17: /* Modulo */
			operation = new MathModuloOperation();
			break;
		case 18: /* Absolute Value */
			operation = new MathAbsoluteOperation();
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
