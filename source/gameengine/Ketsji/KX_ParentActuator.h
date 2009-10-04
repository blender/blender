/**
 * Set or remove an objects parent
 *
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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

#ifndef __KX_PARENTACTUATOR
#define __KX_PARENTACTUATOR

#include "SCA_IActuator.h"
#include "SCA_LogicManager.h"

class KX_ParentActuator : public SCA_IActuator
{
	Py_Header;
	
	/** Mode */
	int m_mode;
	
	/** option */
	bool	m_addToCompound;
	bool	m_ghost;
	/** Object to set as parent */
	SCA_IObject *m_ob;
	
	

 public:
 	enum KX_PARENTACT_MODE
	{
		KX_PARENT_NODEF = 0,
		KX_PARENT_SET,
		KX_PARENT_REMOVE,
		KX_PARENT_MAX

	};
 
	KX_ParentActuator(class SCA_IObject* gameobj,
						int mode,
						bool addToCompound,
						bool ghost,
						SCA_IObject *ob);
	virtual ~KX_ParentActuator();
	virtual bool Update();
	
	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	virtual void Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map);
	virtual bool UnlinkObject(SCA_IObject* clientobj);
	
#ifndef DISABLE_PYTHON

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	/* These are used to get and set m_ob */
	static PyObject* pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	
#endif // DISABLE_PYTHON

}; /* end of class KX_ParentActuator : public SCA_PropertyActuator */

#endif

