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

/** \file KX_MeshProxy.h
 *  \ingroup ketsji
 */

#ifndef __KX_MESHPROXY_H__
#define __KX_MESHPROXY_H__

#ifdef WITH_PYTHON

#include "SCA_IObject.h"

/* utility conversion function */
bool ConvertPythonToMesh(PyObject *value, class RAS_MeshObject **object, bool py_none_ok, const char *error_prefix);

class KX_MeshProxy	: public CValue
{
	Py_Header

	class RAS_MeshObject*	m_meshobj;
public:
	KX_MeshProxy(class RAS_MeshObject* mesh);
	virtual ~KX_MeshProxy();

	void SetMeshModified(bool v);

	// stuff for cvalue related things
	virtual CValue*		Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual const STR_String &	GetText();
	virtual double		GetNumber();
	virtual RAS_MeshObject* GetMesh() { return m_meshobj; }
	virtual STR_String&	GetName();
	virtual void		SetName(const char *name);								// Set the name of the value
	virtual CValue*		GetReplica();

// stuff for python integration

	KX_PYMETHOD(KX_MeshProxy,GetNumMaterials);	// Deprecated
	KX_PYMETHOD(KX_MeshProxy,GetMaterialName);
	KX_PYMETHOD(KX_MeshProxy,GetTextureName);
	KX_PYMETHOD_NOARGS(KX_MeshProxy,GetNumPolygons); // Deprecated
	
	// both take materialid (int)
	KX_PYMETHOD(KX_MeshProxy,GetVertexArrayLength);
	KX_PYMETHOD(KX_MeshProxy,GetVertex);
	KX_PYMETHOD(KX_MeshProxy,GetPolygon);
	KX_PYMETHOD(KX_MeshProxy,Transform);
	KX_PYMETHOD(KX_MeshProxy,TransformUV);
	
	static PyObject *pyattr_get_materials(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject *pyattr_get_numMaterials(void *self, const KX_PYATTRIBUTE_DEF * attrdef);
	static PyObject *pyattr_get_numPolygons(void *self, const KX_PYATTRIBUTE_DEF * attrdef);
};

#endif  /* WITH_PYTHON */

#endif  /* __KX_MESHPROXY_H__ */
