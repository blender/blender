/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef PHYP_PHYSICSOBJECT_WRAPPER
#define PHYP_PHYSICSOBJECT_WRAPPER

#include "Value.h"
#include "PHY_DynamicTypes.h"

class	KX_PhysicsObjectWrapper : public PyObjectPlus
{
	Py_Header;

	PyObject*			_getattr(char* attr);
	virtual int 			_setattr(char *attr, PyObject *value);
public:
	KX_PhysicsObjectWrapper(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsEnvironment* physenv,PyTypeObject *T = &Type);
	virtual ~KX_PhysicsObjectWrapper();
	
	KX_PYMETHOD(KX_PhysicsObjectWrapper , SetPosition);
	KX_PYMETHOD(KX_PhysicsObjectWrapper,SetLinearVelocity);
	KX_PYMETHOD(KX_PhysicsObjectWrapper,SetAngularVelocity);
	KX_PYMETHOD(KX_PhysicsObjectWrapper,SetActive);

private:
	class PHY_IPhysicsController*	m_ctrl;
	PHY_IPhysicsEnvironment* m_physenv;
};

#endif //PHYP_PHYSICSOBJECT_WRAPPER
