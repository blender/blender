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
 */
#ifndef __KX_VERTEXPROXY
#define __KX_VERTEXPROXY

#ifndef DISABLE_PYTHON

#include "SCA_IObject.h"

class KX_VertexProxy	: public CValue
{
	Py_Header;
protected:

	class RAS_TexVert*	m_vertex;
	class KX_MeshProxy*	m_mesh;
public:
	KX_VertexProxy(class KX_MeshProxy*mesh, class RAS_TexVert* vertex);
	virtual ~KX_VertexProxy();

	// stuff for cvalue related things
	CValue*		Calc(VALUE_OPERATOR op, CValue *val) ;
	CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	const STR_String &	GetText();
	double		GetNumber();
	STR_String&	GetName();
	void		SetName(const char *name);								// Set the name of the value
	CValue*		GetReplica();


// stuff for python integration

	KX_PYMETHOD_NOARGS(KX_VertexProxy,GetXYZ);
	KX_PYMETHOD_O(KX_VertexProxy,SetXYZ);
	KX_PYMETHOD_NOARGS(KX_VertexProxy,GetUV);
	KX_PYMETHOD_O(KX_VertexProxy,SetUV);
	
	KX_PYMETHOD_NOARGS(KX_VertexProxy,GetUV2);
	KX_PYMETHOD_VARARGS(KX_VertexProxy,SetUV2);

	KX_PYMETHOD_NOARGS(KX_VertexProxy,GetRGBA);
	KX_PYMETHOD_O(KX_VertexProxy,SetRGBA);
	KX_PYMETHOD_NOARGS(KX_VertexProxy,GetNormal);
	KX_PYMETHOD_O(KX_VertexProxy,SetNormal);

};

#endif // DISABLE_PYTHON

#endif //__KX_VERTEXPROXY

