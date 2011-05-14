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
 * Init bge.shaders
 */

#ifndef __KX_PYSHADERS
#define __KX_PYSHADERS
#ifndef DISABLE_PYTHON

#include "PyObjectPlus.h"

typedef std::vector<class KX_PythonUniform *>	UniformList;

class KX_PythonShader: public PyObjectPlus
{
	Py_Header;

private:
	STR_String m_vert, m_geom, m_frag;
	class KX_BlenderMaterial *m_mat;
	UniformList m_uniforms;

public:
	KX_PythonShader();
	KX_PythonShader(class KX_BlenderMaterial *material);
	~KX_PythonShader();

	STR_String& GetVertex() { return m_vert; }
	STR_String& GetGeometry() { return m_geom; }
	STR_String& GetFragment() { return m_frag; }

	UniformList& GetUniforms() { return m_uniforms; }

	static PyObject* py_shader_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

	KX_PYMETHOD_DOC_O(KX_PythonShader, addUniform);

	static PyObject*	pyattr_get_source(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_source(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_uniforms(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
};

class KX_PythonUniform: public PyObjectPlus
{
	Py_Header;

private:
	STR_String m_name;
	short m_type;
	int m_size;
	void *m_data;

	struct CustomUniform *m_cu;
	bool m_owns_cu; // This is so uniforms created in Python can clean up after themselves

public:
	KX_PythonUniform(char* name, short type, int size);
	KX_PythonUniform(struct CustomUniform *cu);
	~KX_PythonUniform();

	STR_String& GetName() {return m_name;}
	struct CustomUniform *GetCustomUniform() {return m_cu;}

	virtual PyObject* py_repr(void)
	{
		return PyUnicode_FromString(GetName().ReadPtr());
	}

	static PyObject* py_uniform_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

	static PyObject*	pyattr_get_value(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_value(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
};

#endif //ndef DISABLE_PYTHON
#endif // __KX_PYSHADERS