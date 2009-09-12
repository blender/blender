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
 */
#ifndef __KX_POLYROXY
#define __KX_POLYPROXY

#include "SCA_IObject.h"

class KX_PolyProxy	: public CValue
{
	Py_Header;
protected:
	class RAS_Polygon*		m_polygon;
	class RAS_MeshObject*	m_mesh;
public:
	KX_PolyProxy(const class RAS_MeshObject*mesh, class RAS_Polygon* polygon);
	virtual ~KX_PolyProxy();

	// stuff for cvalue related things
	CValue*		Calc(VALUE_OPERATOR op, CValue *val) ;
	CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	const STR_String &	GetText();
	double		GetNumber();
	STR_String&	GetName();
	void		SetName(const char *name);								// Set the name of the value
	CValue*		GetReplica();


// stuff for python integration

	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getMaterialIndex)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getNumVertex)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,isVisible)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,isCollider)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getMaterialName)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getTextureName)
	KX_PYMETHOD_DOC(KX_PolyProxy,getVertexIndex)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getMesh)
	KX_PYMETHOD_DOC_NOARGS(KX_PolyProxy,getMaterial)

};

#endif //__KX_POLYPROXY

