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
 * Ketsji Logic Extenstion: Network Message Actuator class
 */
#ifndef __KX_NETWORKMESSAGEACTUATOR_H
#define __KX_NETWORKMESSAGEACTUATOR_H

#include "STR_String.h"
#include "SCA_IActuator.h"
#include "NG_NetworkMessage.h"

class KX_NetworkMessageActuator : public SCA_IActuator
{
	Py_Header;
	bool m_lastEvent;
	class NG_NetworkScene* m_networkscene;	// needed for replication
	STR_String m_toPropName;
	STR_String m_subject;
	bool m_bPropBody;
	STR_String m_body;
public:
	KX_NetworkMessageActuator(
		SCA_IObject* gameobj,
		NG_NetworkScene* networkscene,
		const STR_String &toPropName,
		const STR_String &subject,
		int bodyType,
		const STR_String &body);
	virtual ~KX_NetworkMessageActuator();

	virtual bool Update();
	virtual CValue* GetReplica();
	virtual void Replace_NetworkScene(NG_NetworkScene *val) 
	{ 
		m_networkscene= val;
	};

	/* ------------------------------------------------------------ */
	/* Python interface ------------------------------------------- */
	/* ------------------------------------------------------------ */

};

#endif //__KX_NETWORKMESSAGEACTUATOR_H

