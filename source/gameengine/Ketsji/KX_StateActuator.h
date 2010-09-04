/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Actuator to toggle visibility/invisibility of objects
 */

#ifndef __KX_STATEACTUATOR
#define __KX_STATEACTUATOR

#include "SCA_IActuator.h"


/*
 * Use of SG_DList : element of actuator being deactivated
 *                   Head: SCA_LogicManager::m_removedActuators
 * Use of SG_QList : element of global activated state actuator list 
 *                   Head: KX_StateActuator::m_stateActuatorHead
 */
class KX_StateActuator : public SCA_IActuator
{
	Py_Header;

	/** Make visible? */
	enum {
		OP_NOP = -1,
		OP_CPY = 0,
		OP_SET,
		OP_CLR,
		OP_NEG,
		OP_COUNT
	};
	// SG_Dlist: element of objects with active actuators, always put in front of the list
	//           Head: SCA_LogicManager::m_activeActuators
	// SG_QList: Head of active state actuators list globally
	//           Elements: KX_StateActuator
	static SG_QList	m_stateActuatorHead;
	int				m_operation;
	int				m_mask;

 public:
	
	KX_StateActuator(
		SCA_IObject* gameobj,
		int operation,
		unsigned int mask
	);

	virtual
		~KX_StateActuator(
			void
			);

	virtual CValue*
		GetReplica(
			void
			);

	virtual bool
		Update();

	virtual void Deactivate();
	virtual void Activate(SG_DList& head);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

};

#endif

