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

#include "COM_ZCombineNode.h"

#include "COM_ZCombineOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_AntiAliasOperation.h"
#include "COM_MixOperation.h"

#include "DNA_material_types.h" // the ramp types

void ZCombineNode::convertToOperations(ExecutionSystem *system, CompositorContext *context)
{
	if ((context->getRenderData()->scemode & R_FULL_SAMPLE) || this->getbNode()->custom2) {
		if (this->getOutputSocket(0)->isConnected()) {
			ZCombineOperation *operation = NULL;
			if (this->getbNode()->custom1) {
				operation = new ZCombineAlphaOperation();
			}
			else {
				operation = new ZCombineOperation();
			}

			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, system);
			this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, system);
			this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2), 2, system);
			this->getInputSocket(3)->relinkConnections(operation->getInputSocket(3), 3, system);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
			system->addOperation(operation);
			if (this->getOutputSocket(1)->isConnected()) {
				MathMinimumOperation *zoperation = new MathMinimumOperation();
				addLink(system, operation->getInputSocket(1)->getConnection()->getFromSocket(), zoperation->getInputSocket(0));
				addLink(system, operation->getInputSocket(3)->getConnection()->getFromSocket(), zoperation->getInputSocket(1));
				this->getOutputSocket(1)->relinkConnections(zoperation->getOutputSocket());
				system->addOperation(zoperation);
			}
		}
		else {
			if (this->getOutputSocket(1)->isConnected()) {
				MathMinimumOperation *zoperation = new MathMinimumOperation();
				this->getInputSocket(1)->relinkConnections(zoperation->getInputSocket(0), 1, system);
				this->getInputSocket(3)->relinkConnections(zoperation->getInputSocket(1), 3, system);
				this->getOutputSocket(1)->relinkConnections(zoperation->getOutputSocket());
				system->addOperation(zoperation);
			}
		}
	}
	else {
		// not full anti alias, use masking for Z combine. be aware it uses anti aliasing.
		// step 1 create mask
		NodeOperation *maskoperation;

		if (this->getbNode()->custom1) {
			maskoperation = new MathGreaterThanOperation();
			this->getInputSocket(1)->relinkConnections(maskoperation->getInputSocket(0), 3, system);
			this->getInputSocket(3)->relinkConnections(maskoperation->getInputSocket(1), 1, system);
		}
		else {
			maskoperation = new MathLessThanOperation();
			this->getInputSocket(1)->relinkConnections(maskoperation->getInputSocket(0), 1, system);
			this->getInputSocket(3)->relinkConnections(maskoperation->getInputSocket(1), 3, system);
		}

		// step 2 anti alias mask bit of an expensive operation, but does the trick
		AntiAliasOperation *antialiasoperation = new AntiAliasOperation();
		addLink(system, maskoperation->getOutputSocket(), antialiasoperation->getInputSocket(0));

		// use mask to blend between the input colors.
		ZCombineMaskOperation *zcombineoperation = this->getbNode()->custom1 ? new ZCombineMaskAlphaOperation() : new ZCombineMaskOperation();
		addLink(system, antialiasoperation->getOutputSocket(), zcombineoperation->getInputSocket(0));
		this->getInputSocket(0)->relinkConnections(zcombineoperation->getInputSocket(1), 0, system);
		this->getInputSocket(2)->relinkConnections(zcombineoperation->getInputSocket(2), 2, system);
		this->getOutputSocket(0)->relinkConnections(zcombineoperation->getOutputSocket());

		system->addOperation(maskoperation);
		system->addOperation(antialiasoperation);
		system->addOperation(zcombineoperation);

		if (this->getOutputSocket(1)->isConnected()) {
			MathMinimumOperation *zoperation = new MathMinimumOperation();
			addLink(system, maskoperation->getInputSocket(0)->getConnection()->getFromSocket(), zoperation->getInputSocket(0));
			addLink(system, maskoperation->getInputSocket(1)->getConnection()->getFromSocket(), zoperation->getInputSocket(1));
			this->getOutputSocket(1)->relinkConnections(zoperation->getOutputSocket());
			system->addOperation(zoperation);
		}
	}
}
