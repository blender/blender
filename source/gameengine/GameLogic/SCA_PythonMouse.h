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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file SCA_PythonMouse.h
 *  \ingroup gamelogic
 */

#ifndef __KX_PYMOUSE
#define __KX_PYMOUSE

#include "PyObjectPlus.h"

class SCA_PythonMouse : public PyObjectPlus
{
	Py_Header
private:
	class SCA_IInputDevice *m_mouse;
	class RAS_ICanvas *m_canvas;
#ifdef WITH_PYTHON
	PyObject* m_event_dict;
#endif
public:
	SCA_PythonMouse(class SCA_IInputDevice* mouse, class RAS_ICanvas* canvas);
	virtual ~SCA_PythonMouse();

	void Show(bool visible);

#ifdef WITH_PYTHON
	KX_PYMETHOD_DOC(SCA_PythonMouse, show);

	static PyObject*	pyattr_get_events(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_active_events(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_position(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_position(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject* value);
	static PyObject*	pyattr_get_visible(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_visible(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject* value);
#endif
};

#endif //__KX_PYMOUSE

