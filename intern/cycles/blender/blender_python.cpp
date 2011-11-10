/*
 * Copyright 2011, Blender Foundation.
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
 */

#include <Python.h>

#include "blender_sync.h"
#include "blender_session.h"

#include "util_opengl.h"
#include "util_path.h"

CCL_NAMESPACE_BEGIN

static PyObject *init_func(PyObject *self, PyObject *args)
{
	const char *path, *user_path;

	if(!PyArg_ParseTuple(args, "ss", &path, &user_path))
		return NULL;
	
	path_init(path, user_path);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *create_func(PyObject *self, PyObject *args)
{
	PyObject *pyengine, *pydata, *pyscene, *pyregion, *pyv3d, *pyrv3d;

	if(!PyArg_ParseTuple(args, "OOOOOO", &pyengine, &pydata, &pyscene, &pyregion, &pyv3d, &pyrv3d))
		return NULL;

	/* RNA */
	PointerRNA engineptr;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(pyengine), &engineptr);
	BL::RenderEngine engine(engineptr);

	PointerRNA dataptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pydata), &dataptr);
	BL::BlendData data(dataptr);

	PointerRNA sceneptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyscene), &sceneptr);
	BL::Scene scene(sceneptr);

	PointerRNA regionptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyregion), &regionptr);
	BL::Region region(regionptr);

	PointerRNA v3dptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyv3d), &v3dptr);
	BL::SpaceView3D v3d(v3dptr);

	PointerRNA rv3dptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyrv3d), &rv3dptr);
	BL::RegionView3D rv3d(rv3dptr);

	/* create session */
	BlenderSession *session;

	if(rv3d) {
		/* interactive session */
		int width = region.width();
		int height = region.height();

		session = new BlenderSession(engine, data, scene, v3d, rv3d, width, height);
	}
	else {
		/* offline session */
		session = new BlenderSession(engine, data, scene);
	}
	
	return PyLong_FromVoidPtr(session);
}

static PyObject *free_func(PyObject *self, PyObject *args)
{
	PyObject *pysession;

	if(!PyArg_ParseTuple(args, "O", &pysession))
		return NULL;

	delete (BlenderSession*)PyLong_AsVoidPtr(pysession);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *render_func(PyObject *self, PyObject *args)
{
	PyObject *pysession;

	if(!PyArg_ParseTuple(args, "O", &pysession))
		return NULL;
	
	Py_BEGIN_ALLOW_THREADS

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);
	session->render();

	Py_END_ALLOW_THREADS

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *draw_func(PyObject *self, PyObject *args)
{
	PyObject *pysession, *pyv3d, *pyrv3d;

	if(!PyArg_ParseTuple(args, "OOO", &pysession, &pyv3d, &pyrv3d))
		return NULL;
	
	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);

	if(PyLong_AsVoidPtr(pyrv3d)) {
		/* 3d view drawing */
		int viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		session->draw(viewport[2], viewport[3]);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *sync_func(PyObject *self, PyObject *args)
{
	PyObject *pysession;

	if(!PyArg_ParseTuple(args, "O", &pysession))
		return NULL;

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);
	session->synchronize();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *available_devices_func(PyObject *self, PyObject *args)
{
	vector<DeviceType> types = Device::available_types();

	PyObject *ret = PyTuple_New(types.size());

	for(size_t i = 0; i < types.size(); i++) {
		string name = Device::string_from_type(types[i]);
		PyTuple_SetItem(ret, i, PyUnicode_FromString(name.c_str()));
	}

	return ret;
}

static PyObject *with_osl_func(PyObject *self, PyObject *args)
{
#ifdef WITH_OSL
	PyObject *ret = Py_True;
#else
	PyObject *ret = Py_False;
#endif

	return Py_INCREF(ret), ret;
}

static PyMethodDef methods[] = {
	{"init", init_func, METH_VARARGS, ""},
	{"create", create_func, METH_VARARGS, ""},
	{"free", free_func, METH_VARARGS, ""},
	{"render", render_func, METH_VARARGS, ""},
	{"draw", draw_func, METH_VARARGS, ""},
	{"sync", sync_func, METH_VARARGS, ""},
	{"available_devices", available_devices_func, METH_NOARGS, ""},
	{"with_osl", with_osl_func, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL},
};

static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"bcycles",
	"Blender cycles render integration",
	-1,
	methods,
	NULL, NULL, NULL, NULL
};

CCL_NAMESPACE_END

extern "C" PyObject *CYCLES_initPython();

PyObject *CYCLES_initPython()
{
	return PyModule_Create(&ccl::module);
}

