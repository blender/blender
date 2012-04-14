/*
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
 */

/** \file KX_PhysicsObjectWrapper.h
 *  \ingroup ketsji
 */

#ifndef __KX_PHYSICSOBJECTWRAPPER_H__
#define __KX_PHYSICSOBJECTWRAPPER_H__

#include "Value.h"
#include "PHY_DynamicTypes.h"

class	KX_PhysicsObjectWrapper : public PyObjectPlus
{
	Py_Header
public:
	KX_PhysicsObjectWrapper(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsEnvironment* physenv);
	virtual ~KX_PhysicsObjectWrapper();
	
#ifdef WITH_PYTHON

	KX_PYMETHOD_VARARGS(KX_PhysicsObjectWrapper,SetPosition);
	KX_PYMETHOD_VARARGS(KX_PhysicsObjectWrapper,SetLinearVelocity);
	KX_PYMETHOD_VARARGS(KX_PhysicsObjectWrapper,SetAngularVelocity);
	KX_PYMETHOD_VARARGS(KX_PhysicsObjectWrapper,SetActive);

#endif // WITH_PYTHON

private:
	class PHY_IPhysicsController*	m_ctrl;
	PHY_IPhysicsEnvironment* m_physenv;
};

#endif //__KX_PHYSICSOBJECTWRAPPER_H__

