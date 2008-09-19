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
 * Ketsji Logic Extenstion: Network Message Sensor class
 */
#ifndef __KX_NETWORKMESSAGE_SENSOR_H
#define __KX_NETWORKMESSAGE_SENSOR_H

#include "SCA_ISensor.h"

class KX_NetworkEventManager;
class NG_NetworkScene;

class KX_NetworkMessageSensor : public SCA_ISensor
{
	// note: Py_Header MUST BE the first listed here
	Py_Header;
	KX_NetworkEventManager *m_Networkeventmgr;
	NG_NetworkScene        *m_NetworkScene;

	// The subject we filter on.
	STR_String m_subject;

	// The number of messages caught since the last frame.
	int m_frame_message_count;

	bool m_IsUp;

	class CListValue* m_BodyList;
	class CListValue* m_SubjectList;
public:
	KX_NetworkMessageSensor(
		KX_NetworkEventManager* eventmgr,	// our eventmanager
		NG_NetworkScene *NetworkScene,		// our scene
		SCA_IObject* gameobj,				// the sensor controlling object
		const STR_String &subject,
		PyTypeObject* T=&Type
	);
	virtual ~KX_NetworkMessageSensor();

	virtual CValue* GetReplica();
	virtual bool Evaluate(CValue* event);
	virtual bool IsPositiveTrigger();
	virtual void Init();
	void EndFrame();
	
	/* ------------------------------------------------------------- */
	/* Python interface -------------------------------------------- */
	/* ------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);

	KX_PYMETHOD_DOC(KX_NetworkMessageSensor, SetSubjectFilterText);
	KX_PYMETHOD_DOC(KX_NetworkMessageSensor, GetFrameMessageCount);
	KX_PYMETHOD_DOC(KX_NetworkMessageSensor, GetBodies);
	KX_PYMETHOD_DOC(KX_NetworkMessageSensor, GetSubject);
	KX_PYMETHOD_DOC(KX_NetworkMessageSensor, GetSubjects);


};

#endif //__KX_NETWORKMESSAGE_SENSOR_H

