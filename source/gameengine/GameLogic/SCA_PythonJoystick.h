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
 * Contributor(s): Mitchell Stokes.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file SCA_PythonJoystick.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_PYTHONJOYSTICK_H__
#define __SCA_PYTHONJOYSTICK_H__

#include "PyObjectPlus.h"

class SCA_PythonJoystick : public PyObjectPlus
{
	Py_Header
private:
	class SCA_Joystick *m_joystick;
#ifdef WITH_PYTHON
	PyObject* m_event_dict;
#endif
public:
	SCA_PythonJoystick(class SCA_Joystick* joystick);
	virtual ~SCA_PythonJoystick();

#ifdef WITH_PYTHON
	virtual PyObject* py_repr(void);

	static PyObject*	pyattr_get_num_x(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_active_buttons(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_hat_values(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_axis_values(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_name(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
#endif
};

#endif //__SCA_PYTHONJOYSTICK_H__

