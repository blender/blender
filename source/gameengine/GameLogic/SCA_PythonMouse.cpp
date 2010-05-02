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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "SCA_PythonMouse.h"
#include "SCA_IInputDevice.h"
#include "RAS_ICanvas.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_PythonMouse::SCA_PythonMouse(SCA_IInputDevice* mouse, RAS_ICanvas* canvas)
: PyObjectPlus(),
m_canvas(canvas),
m_mouse(mouse)
{
}

SCA_PythonMouse::~SCA_PythonMouse()
{
	/* intentionally empty */
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_PythonMouse::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_PythonMouse",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_PythonMouse::Methods[] = {
//	KX_PYMETHODTABLE(SCA_PythonMouse, show),
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_PythonMouse::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("events", SCA_PythonMouse, pyattr_get_events),
	KX_PYATTRIBUTE_RW_FUNCTION("position", SCA_PythonMouse, pyattr_get_position, pyattr_set_position),
	KX_PYATTRIBUTE_RW_FUNCTION("visible", SCA_PythonMouse, pyattr_get_visible, pyattr_set_visible),
	{ NULL }	//Sentinel
};	

PyObject* SCA_PythonMouse::pyattr_get_events(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_PythonMouse* self = static_cast<SCA_PythonMouse*>(self_v);

	PyObject* resultlist = PyList_New(0);

	for (int i=SCA_IInputDevice::KX_BEGINMOUSE; i<=SCA_IInputDevice::KX_ENDMOUSE; i++)
	{
		const SCA_InputEvent & inevent = self->m_mouse->GetEventValue((SCA_IInputDevice::KX_EnumInputs)i);
		
		
		if (inevent.m_status != SCA_InputEvent::KX_NO_INPUTSTATUS)
		{
			PyObject* keypair = PyTuple_New(2);
			PyTuple_SET_ITEM(keypair, 0, PyLong_FromSsize_t(i));
			PyTuple_SET_ITEM(keypair, 1, PyLong_FromSsize_t(inevent.m_status));
			PyList_Append(resultlist, keypair);
		}
	}

	return resultlist;
}

PyObject* SCA_PythonMouse::pyattr_get_position(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_PythonMouse* self = static_cast<SCA_PythonMouse*>(self_v);
	const SCA_InputEvent & xevent = self->m_mouse->GetEventValue(SCA_IInputDevice::KX_MOUSEX);
	const SCA_InputEvent & yevent = self->m_mouse->GetEventValue(SCA_IInputDevice::KX_MOUSEY);

	float x_coord, y_coord;

	x_coord = self->m_canvas->GetMouseNormalizedX(xevent.m_eventval);
	y_coord = self->m_canvas->GetMouseNormalizedY(yevent.m_eventval);

	PyObject* ret = PyTuple_New(2);

	PyTuple_SET_ITEM(ret, 0, PyFloat_FromDouble(x_coord));
	PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(y_coord));

	return ret;
}

int SCA_PythonMouse::pyattr_set_position(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	SCA_PythonMouse* self = static_cast<SCA_PythonMouse*>(self_v);
	int x, y;
	float pyx, pyy;
	if (!PyArg_ParseTuple(value, "ff:position", &pyx, &pyy))
		return PY_SET_ATTR_FAIL;

	x = (int)(pyx*self->m_canvas->GetWidth());
	y = (int)(pyy*self->m_canvas->GetHeight());

	self->m_canvas->SetMousePosition(x, y);

	return PY_SET_ATTR_SUCCESS;
}

PyObject* SCA_PythonMouse::pyattr_get_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_PythonMouse* self = static_cast<SCA_PythonMouse*>(self_v);

	int visible;

	if (self->m_canvas->GetMouseState() == RAS_ICanvas::MOUSE_INVISIBLE)
		visible = 0;
	else
		visible = 1;

	return PyBool_FromLong(visible);
}

int SCA_PythonMouse::pyattr_set_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	SCA_PythonMouse* self = static_cast<SCA_PythonMouse*>(self_v);

	int visible = PyObject_IsTrue(value);

	if (visible == -1)
	{
		PyErr_SetString(PyExc_AttributeError, "SCA_PythonMouse.visible = bool: SCA_PythonMouse, expected True or False");
		return PY_SET_ATTR_FAIL;
	}

	if (visible)
		self->m_canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
	else
		self->m_canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);

	return PY_SET_ATTR_SUCCESS;
}

#endif
