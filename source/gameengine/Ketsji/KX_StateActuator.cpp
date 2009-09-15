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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "KX_StateActuator.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_StateActuator::KX_StateActuator(
	SCA_IObject* gameobj,
	int operation,
	unsigned int mask
	) 
	: SCA_IActuator(gameobj),
	  m_operation(operation),
	  m_mask(mask)
{
	// intentionally empty
}

KX_StateActuator::~KX_StateActuator(
	void
	)
{
	// intentionally empty
}

// used to put state actuator to be executed before any other actuators
SG_QList KX_StateActuator::m_stateActuatorHead;

CValue*
KX_StateActuator::GetReplica(
	void
	)
{
	KX_StateActuator* replica = new KX_StateActuator(*this);
	replica->ProcessReplica();
	return replica;
}

bool
KX_StateActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	unsigned int objMask;

	// execution of state actuator means that we are in the execution phase, reset this pointer
	// because all the active actuator of this object will be removed for sure.
	m_gameobj->m_firstState = NULL;
	RemoveAllEvents();
	if (bNegativeEvent) return false;

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	
	objMask = obj->GetState();
	switch (m_operation) 
	{
	case OP_CPY:
		objMask = m_mask;
		break;
	case OP_SET:
		objMask |= m_mask;
		break;
	case OP_CLR:
		objMask &= ~m_mask;
		break;
	case OP_NEG:
		objMask ^= m_mask;
		break;
	default:
		// unsupported operation, no  nothing
		return false;
	}
	obj->SetState(objMask);
	return false;
}

// this function is only used to deactivate actuators outside the logic loop
// e.g. when an object is deleted.
void KX_StateActuator::Deactivate()
{
	if (QDelink())
	{
		// the actuator was in the active list
		if (m_stateActuatorHead.QEmpty())
			// no more state object active
			m_stateActuatorHead.Delink();
	}
}

void KX_StateActuator::Activate(SG_DList& head)
{
	// sort the state actuators per object on the global list
	if (QEmpty())
	{
		InsertSelfActiveQList(m_stateActuatorHead, &m_gameobj->m_firstState);
		// add front to make sure it runs before other actuators
		head.AddFront(&m_stateActuatorHead);
	}
}


/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_StateActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_StateActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_StateActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_StateActuator::Attributes[] = {
	KX_PYATTRIBUTE_INT_RW("operation",KX_StateActuator::OP_NOP+1,KX_StateActuator::OP_COUNT-1,false,KX_StateActuator,m_operation),
	KX_PYATTRIBUTE_INT_RW("mask",0,0x3FFFFFFF,false,KX_StateActuator,m_mask),
	{ NULL }	//Sentinel
};
