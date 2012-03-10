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

/** \file KX_ConstraintWrapper.h
 *  \ingroup ketsji
 */

#ifndef __KX_CONSTRAINTWRAPPER_H__
#define __KX_CONSTRAINTWRAPPER_H__

#include "Value.h"
#include "PHY_DynamicTypes.h"

class	KX_ConstraintWrapper : public PyObjectPlus
{
	Py_Header
public:
	KX_ConstraintWrapper(PHY_ConstraintType ctype,int constraintId,class PHY_IPhysicsEnvironment* physenv);
	virtual ~KX_ConstraintWrapper ();
	int			getConstraintId() { return m_constraintId; }
	
#ifdef WITH_PYTHON
	KX_PYMETHOD_NOARGS(KX_ConstraintWrapper,GetConstraintId);
	KX_PYMETHOD(KX_ConstraintWrapper,SetParam);
	KX_PYMETHOD(KX_ConstraintWrapper,GetParam);

	static PyObject* pyattr_get_constraintId(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
#endif

private:
	int					m_constraintId;
	PHY_ConstraintType	m_constraintType;
	PHY_IPhysicsEnvironment* m_physenv;
};

#endif //__KX_CONSTRAINTWRAPPER_H__

