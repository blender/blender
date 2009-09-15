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

#include "KX_VisibilityActuator.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_VisibilityActuator::KX_VisibilityActuator(
	SCA_IObject* gameobj,
	bool visible,
	bool occlusion,
	bool recursive
	) 
	: SCA_IActuator(gameobj),
	  m_visible(visible),
	  m_occlusion(occlusion),
	  m_recursive(recursive)
{
	// intentionally empty
}

KX_VisibilityActuator::~KX_VisibilityActuator(
	void
	)
{
	// intentionally empty
}

CValue*
KX_VisibilityActuator::GetReplica(
	void
	)
{
	KX_VisibilityActuator* replica = new KX_VisibilityActuator(*this);
	replica->ProcessReplica();
	return replica;
}

bool
KX_VisibilityActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	
	RemoveAllEvents();
	if (bNegativeEvent) return false;

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	
	obj->SetVisible(m_visible, m_recursive);
	obj->SetOccluder(m_occlusion, m_recursive);
	obj->UpdateBuckets(m_recursive);

	return false;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_VisibilityActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_VisibilityActuator",
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

PyMethodDef KX_VisibilityActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_VisibilityActuator::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("visibility", KX_VisibilityActuator, m_visible),
	KX_PYATTRIBUTE_BOOL_RW("useOcclusion", KX_VisibilityActuator, m_occlusion),
	KX_PYATTRIBUTE_BOOL_RW("useRecursion", KX_VisibilityActuator, m_recursive),
	{ NULL }	//Sentinel
};
