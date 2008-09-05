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
	PyTypeObject* T
	) 
	: SCA_IActuator(gameobj,T),
	  m_visible(visible)
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
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
}

bool
KX_VisibilityActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	
	RemoveAllEvents();
	if (bNegativeEvent) return false;

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	
	obj->SetVisible(m_visible);
	obj->UpdateBuckets();

	return true;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject 
KX_VisibilityActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_VisibilityActuator",
	sizeof(KX_VisibilityActuator),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject 
KX_VisibilityActuator::Parents[] = {
	&KX_VisibilityActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef 
KX_VisibilityActuator::Methods[] = {
	{"set", (PyCFunction) KX_VisibilityActuator::sPySetVisible, 
	 METH_VARARGS, SetVisible_doc},
	{NULL,NULL} //Sentinel
};

PyObject* 
KX_VisibilityActuator::_getattr(
	const STR_String& attr
	) 
{
	_getattr_up(SCA_IActuator);
};



/* set visibility ---------------------------------------------------------- */
char 
KX_VisibilityActuator::SetVisible_doc[] = 
"setVisible(visible?)\n"
"\t - visible? : Make the object visible? (KX_TRUE, KX_FALSE)"
"\tSet the properties of the actuator.\n";
PyObject* 

KX_VisibilityActuator::PySetVisible(PyObject* self, 
				    PyObject* args, 
				    PyObject* kwds) {
	int vis;

	if(!PyArg_ParseTuple(args, "i", &vis)) {
		return NULL;
	}

	m_visible = PyArgToBool(vis);

	Py_Return;
}


