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

/** \file gameengine/GameLogic/SCA_PythonKeyboard.cpp
 *  \ingroup gamelogic
 */


#include "SCA_PythonKeyboard.h"
#include "SCA_IInputDevice.h"

#include "GHOST_C-api.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_PythonKeyboard::SCA_PythonKeyboard(SCA_IInputDevice* keyboard)
: PyObjectPlus(),
m_keyboard(keyboard)
{
#ifdef WITH_PYTHON
	m_event_dict = PyDict_New();
#endif
}

SCA_PythonKeyboard::~SCA_PythonKeyboard()
{
#ifdef WITH_PYTHON
	PyDict_Clear(m_event_dict);
	Py_DECREF(m_event_dict);
#endif
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* clipboard */
static PyObject *gPyGetClipboard(PyObject *args, PyObject *kwds)
{
	char *buf = (char *)GHOST_getClipboard(0);
	return PyUnicode_FromString(buf?buf:"");
}

static PyObject *gPySetClipboard(PyObject *args, PyObject *value)
{
	char* buf;
	if (!PyArg_ParseTuple(value,"s:setClipboard",&buf))
		Py_RETURN_NONE;

	GHOST_putClipboard((GHOST_TInt8 *)buf, 0);
	Py_RETURN_NONE;
}

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_PythonKeyboard::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_PythonKeyboard",
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

PyMethodDef SCA_PythonKeyboard::Methods[] = {
	{"getClipboard", (PyCFunction) gPyGetClipboard, METH_VARARGS, "getCliboard doc"},
	{"setClipboard", (PyCFunction) gPySetClipboard, METH_VARARGS, "setCliboard doc"},
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_PythonKeyboard::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("events", SCA_PythonKeyboard, pyattr_get_events),
	KX_PYATTRIBUTE_RO_FUNCTION("active_events", SCA_PythonKeyboard, pyattr_get_active_events),
	{ NULL }	//Sentinel
};

PyObject *SCA_PythonKeyboard::pyattr_get_events(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_PythonKeyboard* self = static_cast<SCA_PythonKeyboard*>(self_v);
	
	for (int i = SCA_IInputDevice::KX_BEGINKEY; i <= SCA_IInputDevice::KX_ENDKEY; i++) {
		const SCA_InputEvent & inevent = self->m_keyboard->GetEventValue((SCA_IInputDevice::KX_EnumInputs)i);
		PyObject *key   = PyLong_FromLong(i);
		PyObject *value = PyLong_FromLong(inevent.m_status);

		PyDict_SetItem(self->m_event_dict, key, value);

		Py_DECREF(key);
		Py_DECREF(value);
	}
	Py_INCREF(self->m_event_dict);
	return self->m_event_dict;
}

PyObject *SCA_PythonKeyboard::pyattr_get_active_events(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_PythonKeyboard* self = static_cast<SCA_PythonKeyboard*>(self_v);

	PyDict_Clear(self->m_event_dict);
	
	for (int i = SCA_IInputDevice::KX_BEGINKEY; i <= SCA_IInputDevice::KX_ENDKEY; i++) {
		const SCA_InputEvent & inevent = self->m_keyboard->GetEventValue((SCA_IInputDevice::KX_EnumInputs)i);
		
		if (inevent.m_status != SCA_InputEvent::KX_NO_INPUTSTATUS) {
			PyObject *key   = PyLong_FromLong(i);
			PyObject *value = PyLong_FromLong(inevent.m_status);

			PyDict_SetItem(self->m_event_dict, key, value);

			Py_DECREF(key);
			Py_DECREF(value);
		}
	}
	Py_INCREF(self->m_event_dict);
	return self->m_event_dict;
}

#endif
