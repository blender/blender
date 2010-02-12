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
 * Ketsji Logic Extenstion: Network Message Sensor generic implementation
 */

#include "KX_NetworkMessageSensor.h"
#include "KX_NetworkEventManager.h"
#include "NG_NetworkMessage.h"
#include "NG_NetworkScene.h"
#include "NG_NetworkObject.h"
#include "SCA_IObject.h"	
#include "InputParser.h"
#include "ListValue.h"
#include "StringValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NAN_NET_DEBUG
  #include <iostream>
#endif

KX_NetworkMessageSensor::KX_NetworkMessageSensor(
	class KX_NetworkEventManager* eventmgr,	// our eventmanager
	class NG_NetworkScene *NetworkScene,	// our scene
	SCA_IObject* gameobj,					// the sensor controlling object
	const STR_String &subject
) :
    SCA_ISensor(gameobj,eventmgr),
    m_NetworkScene(NetworkScene),
    m_subject(subject),
    m_frame_message_count (0),
    m_BodyList(NULL),
    m_SubjectList(NULL)
{
	Init();
}

void KX_NetworkMessageSensor::Init()
{
    m_IsUp = false;
}

KX_NetworkMessageSensor::~KX_NetworkMessageSensor()
{
}

CValue* KX_NetworkMessageSensor::GetReplica() {
	// This is the standard sensor implementation of GetReplica
	// There may be more network message sensor specific stuff to do here.
	CValue* replica = new KX_NetworkMessageSensor(*this);

	if (replica == NULL) return NULL;
	replica->ProcessReplica();

	return replica;
}

// Return true only for flank (UP and DOWN)
bool KX_NetworkMessageSensor::Evaluate()
{
	bool result = false;
	bool WasUp = m_IsUp;

	m_IsUp = false;

	if (m_BodyList) {
		m_BodyList->Release();
		m_BodyList = NULL;
	}

	if (m_SubjectList) {
		m_SubjectList->Release();
		m_SubjectList = NULL;
	}

	STR_String& toname=GetParent()->GetName();
	STR_String& subject = this->m_subject;

	vector<NG_NetworkMessage*> messages =
		m_NetworkScene->FindMessages(toname,"",subject,true);

	m_frame_message_count = messages.size();

	if (!messages.empty()) {
#ifdef NAN_NET_DEBUG
		printf("KX_NetworkMessageSensor found one or more messages\n");
#endif
		m_IsUp = true;
		m_BodyList = new CListValue();
		m_SubjectList = new CListValue();
	}

	vector<NG_NetworkMessage*>::iterator mesit;
	for (mesit=messages.begin();mesit!=messages.end();mesit++)
	{
		// save the body
		const STR_String& body = (*mesit)->GetMessageText();
		// save the subject
		const STR_String& messub = (*mesit)->GetSubject();
#ifdef NAN_NET_DEBUG
		if (body) {
			cout << "body [" << body << "]\n";
		}
#endif
		m_BodyList->Add(new CStringValue(body,"body"));
		// Store Subject
		m_SubjectList->Add(new CStringValue(messub,"subject"));

		// free the message
		(*mesit)->Release();
	}
	messages.clear();

	result = (WasUp != m_IsUp);

	// Return always true if a message was received otherwise we can loose messages
	if (m_IsUp)
		return true;
	// Is it usefull to return also true when the first frame without a message?? 
	// This will cause a fast on/off cycle that seems useless!
	return result;
}

// return true for being up (no flank needed)
bool KX_NetworkMessageSensor::IsPositiveTrigger()
{
//	printf("KX_NetworkMessageSensor IsPositiveTrigger\n");
	//attempt to fix [ #3809 ] IPO Actuator does not work with some Sensors
	//a better solution is to properly introduce separate Edge and Level triggering concept

	return m_IsUp;
}

#ifndef DISABLE_PYTHON

/* --------------------------------------------------------------------- */
/* Python interface ---------------------------------------------------- */
/* --------------------------------------------------------------------- */

/* Integration hooks --------------------------------------------------- */
PyTypeObject KX_NetworkMessageSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_NetworkMessageSensor",
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
	&SCA_ISensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_NetworkMessageSensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_NetworkMessageSensor::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("subject", 0, 100, false, KX_NetworkMessageSensor, m_subject),
	KX_PYATTRIBUTE_INT_RO("frameMessageCount", KX_NetworkMessageSensor, m_frame_message_count),
	KX_PYATTRIBUTE_RO_FUNCTION("bodies", KX_NetworkMessageSensor, pyattr_get_bodies),
	KX_PYATTRIBUTE_RO_FUNCTION("subjects", KX_NetworkMessageSensor, pyattr_get_subjects),
	{ NULL }	//Sentinel
};

PyObject* KX_NetworkMessageSensor::pyattr_get_bodies(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_NetworkMessageSensor *self = static_cast<KX_NetworkMessageSensor*>(self_v);
	if (self->m_BodyList) {
		return self->m_BodyList->GetProxy();
	} else {
		return (new CListValue())->NewProxy(true);
	}
}

PyObject* KX_NetworkMessageSensor::pyattr_get_subjects(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_NetworkMessageSensor *self = static_cast<KX_NetworkMessageSensor*>(self_v);
	if (self->m_SubjectList) {
		return self->m_SubjectList->GetProxy();
	} else {
		return (new CListValue())->NewProxy(true);
	}
}

#endif // DISABLE_PYTHON
