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
 * Contributor(s): Mitchell Stokes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_LibLoadStatus.cpp
 *  \ingroup bgeconv
 */

#include "KX_LibLoadStatus.h"
#include "PIL_time.h"

KX_LibLoadStatus::KX_LibLoadStatus(class KX_BlenderSceneConverter* kx_converter,
				class KX_KetsjiEngine* kx_engine,
				class KX_Scene* merge_scene,
				const char *path) :
			m_converter(kx_converter),
			m_engine(kx_engine),
			m_mergescene(merge_scene),
			m_data(NULL),
			m_libname(path),
			m_progress(0.f)
#ifdef WITH_PYTHON
			,
			m_finish_cb(NULL),
			m_progress_cb(NULL)
#endif
{
	m_endtime = m_starttime = PIL_check_seconds_timer();
}

void KX_LibLoadStatus::Finish()
{
	m_progress = 1.f;
	m_endtime = PIL_check_seconds_timer();

	RunFinishCallback();
	RunProgressCallback();
}

void KX_LibLoadStatus::RunFinishCallback()
{
#ifdef WITH_PYTHON
	if (m_finish_cb) {
		PyObject* args = Py_BuildValue("(O)", GetProxy());

		if (!PyObject_Call(m_finish_cb, args, NULL)) {
			PyErr_Print();
			PyErr_Clear();
		}

		Py_DECREF(args);
	}
#endif
}

void KX_LibLoadStatus::RunProgressCallback()
{
// Progess callbacks are causing threading problems with Python, so they're disabled for now
#if 0
#ifdef WITH_PYTHON
	if (m_progress_cb) {
		//PyGILState_STATE gstate = PyGILState_Ensure();
		PyObject* args = Py_BuildValue("(O)", GetProxy());

		if (!PyObject_Call(m_progress_cb, args, NULL)) {
			PyErr_Print();
			PyErr_Clear();
		}

		Py_DECREF(args);
		//PyGILState_Release(gstate);
	}
#endif
#endif
}

class KX_BlenderSceneConverter *KX_LibLoadStatus::GetConverter()
{
	return m_converter;
}

class KX_KetsjiEngine *KX_LibLoadStatus::GetEngine()
{
	return m_engine;
}

class KX_Scene *KX_LibLoadStatus::GetMergeScene()
{
	return m_mergescene;
}

void KX_LibLoadStatus::SetLibName(const char *name)
{
	m_libname = name;
}

const char *KX_LibLoadStatus::GetLibName()
{
	return m_libname;
}

void KX_LibLoadStatus::SetData(void *data)
{
	m_data = data;
}

void *KX_LibLoadStatus::GetData()
{
	return m_data;
}

void KX_LibLoadStatus::SetProgress(float progress)
{
	m_progress = progress;
	RunProgressCallback();
}

float KX_LibLoadStatus::GetProgress()
{
	return m_progress;
}

void KX_LibLoadStatus::AddProgress(float progress)
{
	m_progress += progress;
	RunProgressCallback();
}

#ifdef WITH_PYTHON

PyMethodDef KX_LibLoadStatus::Methods[] = 
{
	{NULL} //Sentinel
};

PyAttributeDef KX_LibLoadStatus::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("onFinish", KX_LibLoadStatus, pyattr_get_onfinish, pyattr_set_onfinish),
	// KX_PYATTRIBUTE_RW_FUNCTION("onProgress", KX_LibLoadStatus, pyattr_get_onprogress, pyattr_set_onprogress),
	KX_PYATTRIBUTE_FLOAT_RO("progress", KX_LibLoadStatus, m_progress),
	KX_PYATTRIBUTE_STRING_RO("libraryName", KX_LibLoadStatus, m_libname),
	KX_PYATTRIBUTE_RO_FUNCTION("timeTaken", KX_LibLoadStatus, pyattr_get_timetaken),
	{ NULL }	//Sentinel
};

PyTypeObject KX_LibLoadStatus::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_LibLoadStatus",
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


PyObject* KX_LibLoadStatus::pyattr_get_onfinish(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LibLoadStatus* self = static_cast<KX_LibLoadStatus*>(self_v);
	
	if (self->m_finish_cb) {
		Py_INCREF(self->m_finish_cb);
		return self->m_finish_cb;
	}

	Py_RETURN_NONE;
}

int KX_LibLoadStatus::pyattr_set_onfinish(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LibLoadStatus* self = static_cast<KX_LibLoadStatus*>(self_v);

	if (!PyCallable_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "KX_LibLoadStatus.onFinished requires a callable object");
		return PY_SET_ATTR_FAIL;
	}

	if (self->m_finish_cb)
		Py_DECREF(self->m_finish_cb);

	Py_INCREF(value);
	self->m_finish_cb = value;

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_LibLoadStatus::pyattr_get_onprogress(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LibLoadStatus* self = static_cast<KX_LibLoadStatus*>(self_v);
	
	if (self->m_progress_cb) {
		Py_INCREF(self->m_progress_cb);
		return self->m_progress_cb;
	}

	Py_RETURN_NONE;
}

int KX_LibLoadStatus::pyattr_set_onprogress(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LibLoadStatus* self = static_cast<KX_LibLoadStatus*>(self_v);

	if (!PyCallable_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "KX_LibLoadStatus.onProgress requires a callable object");
		return PY_SET_ATTR_FAIL;
	}

	if (self->m_progress_cb)
		Py_DECREF(self->m_progress_cb);

	Py_INCREF(value);
	self->m_progress_cb = value;

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_LibLoadStatus::pyattr_get_timetaken(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LibLoadStatus* self = static_cast<KX_LibLoadStatus*>(self_v);

	return PyFloat_FromDouble(self->m_endtime - self->m_starttime);
}
#endif // WITH_PYTHON
