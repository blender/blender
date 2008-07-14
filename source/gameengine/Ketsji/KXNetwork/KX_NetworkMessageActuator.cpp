/**
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
 * Ketsji Logic Extenstion: Network Message Actuator generic implementation
 */

#include "NG_NetworkScene.h"
#include "KX_NetworkMessageActuator.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

KX_NetworkMessageActuator::KX_NetworkMessageActuator(
	SCA_IObject* gameobj,		// the actuator controlling object
	NG_NetworkScene* networkscene,	// needed for replication
	const STR_String &toPropName,
	const STR_String &subject,
	int bodyType,
	const STR_String &body,
	PyTypeObject* T) :
	SCA_IActuator(gameobj,T),
	m_networkscene(networkscene),
	m_toPropName(toPropName),
	m_subject(subject),
	m_bodyType(bodyType),
	m_body(body)
{
}

KX_NetworkMessageActuator::~KX_NetworkMessageActuator()
{
}

// returns true if the actuators needs to be running over several frames
bool KX_NetworkMessageActuator::Update()
{
	//printf("update messageactuator\n");
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent) {
		return false; // do nothing on negative events
		//printf("messageactuator false event\n");
	}
	//printf("messageactuator true event\n");

	if (m_bodyType == 1) // ACT_MESG_PROP in DNA_actuator_types.h
	{
		m_networkscene->SendMessage(
			m_toPropName,
			GetParent()->GetName(),
			m_subject,
			GetParent()->GetPropertyText(m_body,""));
	} else
	{
		m_networkscene->SendMessage(
			m_toPropName,
			GetParent()->GetName(),
			m_subject,
			m_body);
	}
	return false;
}

CValue* KX_NetworkMessageActuator::GetReplica()
{
	KX_NetworkMessageActuator* replica =
	    new KX_NetworkMessageActuator(*this);
	replica->ProcessReplica();

	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}

/* -------------------------------------------------------------------- */
/* Python interface --------------------------------------------------- */
/* -------------------------------------------------------------------- */

/* Integration hooks -------------------------------------------------- */
PyTypeObject KX_NetworkMessageActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_NetworkMessageActuator",
	sizeof(KX_NetworkMessageActuator),
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

PyParentObject KX_NetworkMessageActuator::Parents[] = {
	&KX_NetworkMessageActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_NetworkMessageActuator::Methods[] = {
	{"setToPropName", (PyCFunction)
		KX_NetworkMessageActuator::sPySetToPropName, METH_VARARGS},
	{"setSubject", (PyCFunction)
		KX_NetworkMessageActuator::sPySetSubject, METH_VARARGS},
	{"setBodyType", (PyCFunction)
		KX_NetworkMessageActuator::sPySetBodyType, METH_VARARGS},
	{"setBody", (PyCFunction)
		KX_NetworkMessageActuator::sPySetBody, METH_VARARGS},
	{NULL,NULL} // Sentinel
};

PyObject* KX_NetworkMessageActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}

// 1. SetToPropName
PyObject* KX_NetworkMessageActuator::PySetToPropName(
	PyObject* self,
	PyObject* args,
	PyObject* kwds)
{
    char* ToPropName;

	if (PyArg_ParseTuple(args, "s", &ToPropName)) {
	     m_toPropName = ToPropName;
	}
	else {
		return NULL;
	}

	Py_Return;
}

// 2. SetSubject
PyObject* KX_NetworkMessageActuator::PySetSubject(
	PyObject* self,
	PyObject* args,
	PyObject* kwds)
{
    char* Subject;

	if (PyArg_ParseTuple(args, "s", &Subject)) {
	     m_subject = Subject;
	}
	else {
		return NULL;
	}
	
	Py_Return;
}

// 3. SetBodyType
PyObject* KX_NetworkMessageActuator::PySetBodyType(
	PyObject* self,
	PyObject* args,
	PyObject* kwds)
{
    int BodyType;

	if (PyArg_ParseTuple(args, "i", &BodyType)) {
	     m_bodyType = BodyType;
	}
	else {
		return NULL;
	}

	Py_Return;
}

// 4. SetBody
PyObject* KX_NetworkMessageActuator::PySetBody(
	PyObject* self,
	PyObject* args,
	PyObject* kwds)
{
    char* Body;

	if (PyArg_ParseTuple(args, "s", &Body)) {
	     m_body = Body;
	}
	else {
		return NULL;
	}

	Py_Return;
}

