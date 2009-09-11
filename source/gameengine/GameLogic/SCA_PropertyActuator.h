/**
 * SCA_PropertyActuator.h
 *
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
 */

#ifndef __KX_PROPERTYACTUATOR
#define __KX_PROPERTYACTUATOR

#include "SCA_IActuator.h"

class SCA_PropertyActuator : public SCA_IActuator
{
	Py_Header;
	
	enum KX_ACT_PROP_MODE {
		KX_ACT_PROP_NODEF = 0,
		KX_ACT_PROP_ASSIGN,
		KX_ACT_PROP_ADD,
		KX_ACT_PROP_COPY,
		KX_ACT_PROP_TOGGLE,
		KX_ACT_PROP_MAX
	};
	
	/**check whether this value is valid */
	bool isValid(KX_ACT_PROP_MODE mode);
	
	int			m_type;
	STR_String	m_propname;
	STR_String	m_exprtxt;
	SCA_IObject* m_sourceObj; // for copy property actuator

public:



	SCA_PropertyActuator(
		SCA_IObject* gameobj,
		SCA_IObject* sourceObj,
		const STR_String& propname,
		const STR_String& expr,
		int acttype);


	~SCA_PropertyActuator();

		CValue* 
	GetReplica(
	);

	virtual void ProcessReplica();
	virtual bool UnlinkObject(SCA_IObject* clientobj);
	virtual void Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map);

	virtual bool 
	Update();

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
};

#endif //__KX_PROPERTYACTUATOR_DOC

