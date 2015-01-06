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
 * Contributor(s): Geoffrey Gollmer, Jorge Bernal
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "KX_MouseActuator.h"
#include "KX_KetsjiEngine.h"
#include "SCA_MouseManager.h"
#include "SCA_IInputDevice.h"
#include "RAS_ICanvas.h"
#include "KX_GameObject.h"
#include "MT_Vector3.h"
#include "MT_Scalar.h"
#include "MT_assert.h"
#include "limits.h"

#include "BLI_math.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_MouseActuator::KX_MouseActuator(
	SCA_IObject* gameobj,

	KX_KetsjiEngine* ketsjiEngine,
	SCA_MouseManager* eventmgr,
	int acttype,
	bool visible,
	bool* use_axis,
	float* threshold,
	bool* reset,
	int* object_axis,
	bool* local,
	float* sensitivity,
	float* limit_x,
	float* limit_y
):
	SCA_IActuator(gameobj, KX_ACT_MOUSE),
	m_ketsji(ketsjiEngine),
	m_eventmgr(eventmgr),
	m_type(acttype),
	m_visible(visible),
	m_use_axis_x(use_axis[0]),
	m_use_axis_y(use_axis[1]),
	m_reset_x(reset[0]),
	m_reset_y(reset[1]),
	m_local_x(local[0]),
	m_local_y(local[1])
{
	m_canvas = m_ketsji->GetCanvas();
	m_oldposition[0] = m_oldposition[1] = -1.f;
	m_limit_x[0] = limit_x[0];
	m_limit_x[1] = limit_x[1];
	m_limit_y[0] = limit_y[0];
	m_limit_y[1] = limit_y[1];
	m_threshold[0] = threshold[0];
	m_threshold[1] = threshold[1];
	m_object_axis[0] = object_axis[0];
	m_object_axis[1] = object_axis[1];
	m_sensitivity[0] = sensitivity[0];
	m_sensitivity[1] = sensitivity[1];
	m_angle[0] = 0.f;
	m_angle[1] = 0.f;
}

KX_MouseActuator::~KX_MouseActuator()
{
}

bool KX_MouseActuator::Update()
{
	bool result = false;

	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	KX_GameObject *parent = static_cast<KX_GameObject *>(GetParent());

	m_mouse = ((SCA_MouseManager *)m_eventmgr)->GetInputDevice();

	switch (m_type) {
		case KX_ACT_MOUSE_VISIBILITY:
		{
			if (m_visible) {
				if (m_canvas) {
					m_canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
				}
			}
			else {
				if (m_canvas) {
					m_canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
				}
			}
			break;
		}
		case KX_ACT_MOUSE_LOOK:
		{
			if (m_mouse) {

				float position[2];
				float movement[2];
				MT_Vector3 rotation;
				float setposition[2] = {0.0};
				float center_x = 0.5, center_y = 0.5;

				getMousePosition(position);

				movement[0] = position[0];
				movement[1] = position[1];

				//preventing undesired drifting when resolution is odd
				if ((m_canvas->GetWidth() % 2) != 0) {
					center_x = ((m_canvas->GetWidth() - 1.0) / 2.0) / (m_canvas->GetWidth());
				}
				if ((m_canvas->GetHeight() % 2) != 0) {
				    center_y = ((m_canvas->GetHeight() - 1.0) / 2.0) / (m_canvas->GetHeight());
				}

				//preventing initial skipping.
				if ((m_oldposition[0] <= -0.9) && (m_oldposition[1] <= -0.9)) {

					if (m_reset_x) {
						m_oldposition[0] = center_x;
					}
					else {
						m_oldposition[0] = position[0];
					}

					if (m_reset_y) {
						m_oldposition[1] = center_y;
					}
					else {
						m_oldposition[1] = position[1];
					}
					setMousePosition(m_oldposition[0], m_oldposition[1]);
					break;
				}

				//Calculating X axis.
				if (m_use_axis_x) {

					if (m_reset_x) {
						setposition[0] = center_x;
						movement[0] -= center_x;
					}
					else {
						setposition[0] = position[0];
						movement[0] -= m_oldposition[0];
					}

					movement[0] *= -1.0;

					/* Don't apply the rotation when we are under a certain threshold for mouse
					  movement */

					if (((movement[0] > (m_threshold[0] / 10.0)) ||
					    ((movement[0] * (-1.0)) > (m_threshold[0] / 10.0)))) {

						movement[0] *= m_sensitivity[0];

						if ((m_limit_x[0] != 0.0) && ((m_angle[0] + movement[0]) <= m_limit_x[0])) {
							movement[0] = m_limit_x[0] - m_angle[0];
						}

						if ((m_limit_x[1] != 0.0) && ((m_angle[0] + movement[0]) >= m_limit_x[1])) {
							movement[0] = m_limit_x[1] - m_angle[0];
						}

						m_angle[0] += movement[0];

						switch (m_object_axis[0]) {
							case KX_ACT_MOUSE_OBJECT_AXIS_X:
							{
								rotation = MT_Vector3(movement[0], 0.0, 0.0);
								break;
							}
							case KX_ACT_MOUSE_OBJECT_AXIS_Y:
							{
								rotation = MT_Vector3(0.0, movement[0], 0.0);
								break;
							}
							case KX_ACT_MOUSE_OBJECT_AXIS_Z:
							{
								rotation = MT_Vector3(0.0, 0.0, movement[0]);
								break;
							}
							default:
								break;
						}
						parent->ApplyRotation(rotation, m_local_x);
					}
				}
				else {
					setposition[0] = center_x;
				}

				//Calculating Y axis.
				if (m_use_axis_y) {

					if (m_reset_y) {
						setposition[1] = center_y;
						movement[1] -= center_y;
					}
					else {
						setposition[1] = position[1];
						movement[1] -= m_oldposition[1];
					}

					movement[1] *= -1.0;

					/* Don't apply the rotation when we are under a certain threshold for mouse
					  movement */

					if (((movement[1] > (m_threshold[1] / 10.0)) ||
					    ((movement[1] * (-1.0)) > (m_threshold[1] / 10.0)))) {

						movement[1] *= m_sensitivity[1];

						if ((m_limit_y[0] != 0.0) && ((m_angle[1] + movement[1]) <= m_limit_y[0])) {
							movement[1] = m_limit_y[0] - m_angle[1];
						}

						if ((m_limit_y[1] != 0.0) && ((m_angle[1] + movement[1]) >= m_limit_y[1])) {
							movement[1] = m_limit_y[1] - m_angle[1];
						}

						m_angle[1] += movement[1];

						switch (m_object_axis[1])
						{
							case KX_ACT_MOUSE_OBJECT_AXIS_X:
							{
								rotation = MT_Vector3(movement[1], 0.0, 0.0);
								break;
							}
							case KX_ACT_MOUSE_OBJECT_AXIS_Y:
							{
								rotation = MT_Vector3(0.0, movement[1], 0.0);
								break;
							}
							case KX_ACT_MOUSE_OBJECT_AXIS_Z:
							{
								rotation = MT_Vector3(0.0, 0.0, movement[1]);
								break;
							}
							default:
								break;
						}
						parent->ApplyRotation(rotation, m_local_y);
					}
				}
				else {
					setposition[1] = center_y;
				}

				setMousePosition(setposition[0], setposition[1]);

				m_oldposition[0] = position[0];
				m_oldposition[1] = position[1];

			}
			else {
				//printf("\nNo input device detected for mouse actuator\n");
			}
			break;
		}
		default:
			break;
		}
	return result;
}

bool KX_MouseActuator::isValid(KX_MouseActuator::KX_ACT_MOUSE_MODE mode)
{
	return ((mode > KX_ACT_MOUSE_NODEF) && (mode < KX_ACT_MOUSE_MAX));
}


CValue* KX_MouseActuator::GetReplica()
{
	KX_MouseActuator* replica = new KX_MouseActuator(*this);

	replica->ProcessReplica();
	return replica;
}

void KX_MouseActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
}

void KX_MouseActuator::getMousePosition(float* pos)
{
	MT_assert(!m_mouse);
	const SCA_InputEvent & xevent = m_mouse->GetEventValue(SCA_IInputDevice::KX_MOUSEX);
	const SCA_InputEvent & yevent = m_mouse->GetEventValue(SCA_IInputDevice::KX_MOUSEY);

	pos[0] = m_canvas->GetMouseNormalizedX(xevent.m_eventval);
	pos[1] = m_canvas->GetMouseNormalizedY(yevent.m_eventval);
}

void KX_MouseActuator::setMousePosition(float fx, float fy)
{
	int x, y;

	x = (int)(fx * m_canvas->GetWidth());
	y = (int)(fy * m_canvas->GetHeight());

	m_canvas->SetMousePosition(x, y);
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_MouseActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_MouseActuator",
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
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_MouseActuator::Methods[] = {
	{"reset", (PyCFunction) KX_MouseActuator::sPyReset, METH_NOARGS,"reset() : undo rotation caused by actuator\n"},
	{NULL,NULL} //Sentinel
};



PyAttributeDef KX_MouseActuator::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("visible", KX_MouseActuator, m_visible),
	KX_PYATTRIBUTE_BOOL_RW("use_axis_x", KX_MouseActuator, m_use_axis_x),
	KX_PYATTRIBUTE_BOOL_RW("use_axis_y", KX_MouseActuator, m_use_axis_y),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RW("threshold", 0.0, 0.5, KX_MouseActuator, m_threshold, 2),
	KX_PYATTRIBUTE_BOOL_RW("reset_x", KX_MouseActuator, m_reset_x),
	KX_PYATTRIBUTE_BOOL_RW("reset_y", KX_MouseActuator, m_reset_y),
	KX_PYATTRIBUTE_INT_ARRAY_RW("object_axis", 0, 2, 1, KX_MouseActuator, m_object_axis, 2),
	KX_PYATTRIBUTE_BOOL_RW("local_x", KX_MouseActuator, m_local_x),
	KX_PYATTRIBUTE_BOOL_RW("local_y", KX_MouseActuator, m_local_y),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RW("sensitivity", -FLT_MAX, FLT_MAX, KX_MouseActuator, m_sensitivity, 2),
	KX_PYATTRIBUTE_RW_FUNCTION("limit_x", KX_MouseActuator, pyattr_get_limit_x, pyattr_set_limit_x),
	KX_PYATTRIBUTE_RW_FUNCTION("limit_y", KX_MouseActuator, pyattr_get_limit_y, pyattr_set_limit_y),
	KX_PYATTRIBUTE_RW_FUNCTION("angle", KX_MouseActuator, pyattr_get_angle, pyattr_set_angle),
	{ NULL }	//Sentinel
};

PyObject* KX_MouseActuator::pyattr_get_limit_x(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);
	return Py_BuildValue("[f,f]", (self->m_limit_x[0] / M_PI * 180.0), (self->m_limit_x[1] / M_PI * 180.0));
}

int KX_MouseActuator::pyattr_set_limit_x(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	PyObject *item1, *item2;
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);

	if (!PyList_Check(value))
		return PY_SET_ATTR_FAIL;

	if (PyList_Size(value) != 2)
		return PY_SET_ATTR_FAIL;

	item1 = PyList_GET_ITEM(value, 0);
	item2 = PyList_GET_ITEM(value, 1);

	if (!(PyFloat_Check(item1)) || !(PyFloat_Check(item2))) {
		return PY_SET_ATTR_FAIL;
	}
	else {
		self->m_limit_x[0] = (PyFloat_AsDouble(item1) * M_PI / 180.0);
		self->m_limit_x[1] = (PyFloat_AsDouble(item2) * M_PI / 180.0);
	}

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_MouseActuator::pyattr_get_limit_y(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);
	return Py_BuildValue("[f,f]", (self->m_limit_y[0] / M_PI * 180.0), (self->m_limit_y[1] / M_PI * 180.0));
}

int KX_MouseActuator::pyattr_set_limit_y(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	PyObject *item1, *item2;
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);

	if (!PyList_Check(value))
		return PY_SET_ATTR_FAIL;

	if (PyList_Size(value) != 2)
		return PY_SET_ATTR_FAIL;

	item1 = PyList_GET_ITEM(value, 0);
	item2 = PyList_GET_ITEM(value, 1);

	if (!(PyFloat_Check(item1)) || !(PyFloat_Check(item2))) {
		return PY_SET_ATTR_FAIL;
	}
	else {
		self->m_limit_y[0] = (PyFloat_AsDouble(item1) * M_PI / 180.0);
		self->m_limit_y[1] = (PyFloat_AsDouble(item2) * M_PI / 180.0);
	}

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_MouseActuator::pyattr_get_angle(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);
	return Py_BuildValue("[f,f]", (self->m_angle[0] / M_PI * 180.0), (self->m_angle[1] / M_PI * 180.0));
}

int KX_MouseActuator::pyattr_set_angle(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	PyObject *item1, *item2;
	KX_MouseActuator* self= static_cast<KX_MouseActuator*>(self_v);

	if (!PyList_Check(value))
		return PY_SET_ATTR_FAIL;

	if (PyList_Size(value) != 2)
		return PY_SET_ATTR_FAIL;

	item1 = PyList_GET_ITEM(value, 0);
	item2 = PyList_GET_ITEM(value, 1);

	if (!(PyFloat_Check(item1)) || !(PyFloat_Check(item2))) {
		return PY_SET_ATTR_FAIL;
	}
	else {
		self->m_angle[0] = (PyFloat_AsDouble(item1) * M_PI / 180.0);
		self->m_angle[1] = (PyFloat_AsDouble(item2) * M_PI / 180.0);
	}

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_MouseActuator::PyReset()
{
	MT_Vector3 rotation;
	KX_GameObject *parent = static_cast<KX_GameObject *>(GetParent());

	switch (m_object_axis[0]) {
		case KX_ACT_MOUSE_OBJECT_AXIS_X:
		{
			rotation = MT_Vector3(-1.0 * m_angle[0], 0.0, 0.0);
			break;
		}
		case KX_ACT_MOUSE_OBJECT_AXIS_Y:
		{
			rotation = MT_Vector3(0.0, -1.0 * m_angle[0], 0.0);
			break;
		}
		case KX_ACT_MOUSE_OBJECT_AXIS_Z:
		{
			rotation = MT_Vector3(0.0, 0.0, -1.0 * m_angle[0]);
			break;
		}
		default:
			break;
	}
	parent->ApplyRotation(rotation, m_local_x);

	switch (m_object_axis[1]) {
		case KX_ACT_MOUSE_OBJECT_AXIS_X:
		{
			rotation = MT_Vector3(-1.0 * m_angle[1], 0.0, 0.0);
			break;
		}
		case KX_ACT_MOUSE_OBJECT_AXIS_Y:
		{
			rotation = MT_Vector3(0.0, -1.0 * m_angle[1], 0.0);
			break;
		}
		case KX_ACT_MOUSE_OBJECT_AXIS_Z:
		{
			rotation = MT_Vector3(0.0, 0.0, -1.0 * m_angle[1]);
			break;
		}
		default:
			break;
	}
	parent->ApplyRotation(rotation, m_local_y);

	m_angle[0] = 0.0;
	m_angle[1] = 0.0;

	Py_RETURN_NONE;
}

#endif
