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

#include "CCL_api.h"

#include "blender_sync.h"
#include "blender_session.h"

#include "util_foreach.h"
#include "util_md5.h"
#include "util_opengl.h"
#include "util_path.h"

#ifdef WITH_OSL
#include "osl.h"

#include <OSL/oslquery.h>
#include <OSL/oslconfig.h>
#endif

CCL_NAMESPACE_BEGIN

static PyObject *init_func(PyObject *self, PyObject *args)
{
	const char *path, *user_path;

	if(!PyArg_ParseTuple(args, "ss", &path, &user_path))
		return NULL;
	
	path_init(path, user_path);

	Py_RETURN_NONE;
}

static PyObject *create_func(PyObject *self, PyObject *args)
{
	PyObject *pyengine, *pyuserpref, *pydata, *pyscene, *pyregion, *pyv3d, *pyrv3d;

	if(!PyArg_ParseTuple(args, "OOOOOOO", &pyengine, &pyuserpref, &pydata, &pyscene, &pyregion, &pyv3d, &pyrv3d))
		return NULL;

	/* RNA */
	PointerRNA engineptr;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(pyengine), &engineptr);
	BL::RenderEngine engine(engineptr);

	PointerRNA userprefptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyuserpref), &userprefptr);
	BL::UserPreferences userpref(userprefptr);

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

	Py_BEGIN_ALLOW_THREADS

	if(rv3d) {
		/* interactive session */
		int width = region.width();
		int height = region.height();

		session = new BlenderSession(engine, userpref, data, scene, v3d, rv3d, width, height);
	}
	else {
		/* offline session */
		session = new BlenderSession(engine, userpref, data, scene);
	}

	Py_END_ALLOW_THREADS

	return PyLong_FromVoidPtr(session);
}

static PyObject *free_func(PyObject *self, PyObject *value)
{
	delete (BlenderSession*)PyLong_AsVoidPtr(value);

	Py_RETURN_NONE;
}

static PyObject *render_func(PyObject *self, PyObject *value)
{
	Py_BEGIN_ALLOW_THREADS

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);
	session->render();

	Py_END_ALLOW_THREADS

	Py_RETURN_NONE;
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

	Py_RETURN_NONE;
}

static PyObject *sync_func(PyObject *self, PyObject *value)
{
	Py_BEGIN_ALLOW_THREADS

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);
	session->synchronize();

	Py_END_ALLOW_THREADS

	Py_RETURN_NONE;
}

static PyObject *available_devices_func(PyObject *self, PyObject *args)
{
	vector<DeviceInfo>& devices = Device::available_devices();
	PyObject *ret = PyTuple_New(devices.size());

	for(size_t i = 0; i < devices.size(); i++) {
		DeviceInfo& device = devices[i];
		PyTuple_SET_ITEM(ret, i, PyUnicode_FromString(device.description.c_str()));
	}

	return ret;
}

#ifdef WITH_OSL
static PyObject *osl_update_node_func(PyObject *self, PyObject *args)
{
	PyObject *pynodegroup, *pynode;
	const char *filepath = NULL;

	if(!PyArg_ParseTuple(args, "OOs", &pynodegroup, &pynode, &filepath))
		return NULL;

	/* RNA */
	PointerRNA nodeptr;
	RNA_pointer_create((ID*)PyLong_AsVoidPtr(pynodegroup), &RNA_ShaderNodeScript, (void*)PyLong_AsVoidPtr(pynode), &nodeptr);
	BL::ShaderNodeScript b_node(nodeptr);

	/* update bytecode hash */
	string bytecode = b_node.bytecode();

	if(!bytecode.empty()) {
		MD5Hash md5;
		md5.append((const uint8_t*)bytecode.c_str(), bytecode.size());
		b_node.bytecode_hash(md5.get_hex().c_str());
	}
	else
		b_node.bytecode_hash("");

	/* query from file path */
	OSL::OSLQuery query;

	if(!OSLShaderManager::osl_query(query, filepath))
		Py_RETURN_FALSE;

	/* add new sockets from parameters */
	set<void*> used_sockets;

	for(int i = 0; i < query.nparams(); i++) {
		const OSL::OSLQuery::Parameter *param = query.getparam(i);

		/* skip unsupported types */
		if(param->varlenarray || param->isstruct || param->type.arraylen > 1)
			continue;

		/* determine socket type */
		BL::NodeSocket::type_enum socket_type;
		float default_float4[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		float default_float = 0.0f;
		int default_int = 0;
		std::string default_string = "";
		
		if(param->isclosure) {
			socket_type = BL::NodeSocket::type_SHADER;
		}
		else if(param->type.vecsemantics == TypeDesc::COLOR) {
			socket_type = BL::NodeSocket::type_RGBA;

			if(param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		}
		else if(param->type.vecsemantics == TypeDesc::POINT ||
		        param->type.vecsemantics == TypeDesc::VECTOR ||
		        param->type.vecsemantics == TypeDesc::NORMAL) {
			socket_type = BL::NodeSocket::type_VECTOR;

			if(param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		}
		else if(param->type.aggregate == TypeDesc::SCALAR) {
			if(param->type.basetype == TypeDesc::INT) {
				socket_type = BL::NodeSocket::type_INT;
				if(param->validdefault)
					default_int = param->idefault[0];
			}
			else if(param->type.basetype == TypeDesc::FLOAT) {
				socket_type = BL::NodeSocket::type_VALUE;
				if(param->validdefault)
					default_float = param->fdefault[0];
			}
			else if(param->type.basetype == TypeDesc::STRING) {
				socket_type =  BL::NodeSocket::type_STRING;
				if(param->validdefault)
					default_string = param->sdefault[0];
			}
		}
		else
			continue;

		/* find socket socket */
		BL::NodeSocket b_sock = b_node.find_socket(param->name.c_str(), param->isoutput);

		/* remove if type no longer matches */
		if(b_sock && b_sock.type() != socket_type) {
			b_node.remove_socket(b_sock);
			b_sock = BL::NodeSocket(PointerRNA_NULL);
		}

		/* create new socket */
		if(!b_sock) {
			b_sock = b_node.add_socket(param->name.c_str(), socket_type, param->isoutput);

			/* set default value */
			if(socket_type == BL::NodeSocket::type_VALUE) {
				BL::NodeSocketFloatNone b_float_sock(b_sock.ptr);
				b_float_sock.default_value(default_float);
			}
			else if(socket_type == BL::NodeSocket::type_INT) {
				BL::NodeSocketIntNone b_int_sock(b_sock.ptr);
				b_int_sock.default_value(default_int);
			}
			else if(socket_type == BL::NodeSocket::type_RGBA) {
				BL::NodeSocketRGBA b_rgba_sock(b_sock.ptr);
				b_rgba_sock.default_value(default_float4);
			}
			else if(socket_type == BL::NodeSocket::type_VECTOR) {
				BL::NodeSocketVectorNone b_vector_sock(b_sock.ptr);
				b_vector_sock.default_value(default_float4);
			}
			else if(socket_type == BL::NodeSocket::type_STRING) {
				BL::NodeSocketStringNone b_string_sock(b_sock.ptr);
				b_string_sock.default_value(default_string);
			}
		}

		used_sockets.insert(b_sock.ptr.data);
	}

	/* remove unused parameters */
	bool removed;

	do {
		BL::Node::inputs_iterator b_input;
		BL::Node::outputs_iterator b_output;

		removed = false;

		for (b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
			if(used_sockets.find(b_input->ptr.data) == used_sockets.end()) {
				b_node.remove_socket(*b_input);
				removed = true;
				break;
			}
		}

		for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if(used_sockets.find(b_output->ptr.data) == used_sockets.end()) {
				b_node.remove_socket(*b_output);
				removed = true;
				break;
			}
		}
	} while(removed);

	Py_RETURN_TRUE;
}

static PyObject *osl_compile_func(PyObject *self, PyObject *args)
{
	const char *inputfile = NULL, *outputfile = NULL;

	if(!PyArg_ParseTuple(args, "ss", &inputfile, &outputfile))
		return NULL;
	
	/* return */
	if(!OSLShaderManager::osl_compile(inputfile, outputfile))
		Py_RETURN_FALSE;

	Py_RETURN_TRUE;
}
#endif

static PyMethodDef methods[] = {
	{"init", init_func, METH_VARARGS, ""},
	{"create", create_func, METH_VARARGS, ""},
	{"free", free_func, METH_O, ""},
	{"render", render_func, METH_O, ""},
	{"draw", draw_func, METH_VARARGS, ""},
	{"sync", sync_func, METH_O, ""},
#ifdef WITH_OSL
	{"osl_update_node", osl_update_node_func, METH_VARARGS, ""},
	{"osl_compile", osl_compile_func, METH_VARARGS, ""},
#endif
	{"available_devices", available_devices_func, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL},
};

static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"_cycles",
	"Blender cycles render integration",
	-1,
	methods,
	NULL, NULL, NULL, NULL
};

static CCLDeviceInfo *compute_device_list(DeviceType type)
{
	/* device list stored static */
	static ccl::vector<CCLDeviceInfo> device_list;
	static ccl::DeviceType device_type = DEVICE_NONE;

	/* create device list if it's not already done */
	if(type != device_type) {
		ccl::vector<DeviceInfo>& devices = ccl::Device::available_devices();

		device_type = type;
		device_list.clear();

		/* add devices */
		int i = 0;

		foreach(DeviceInfo& info, devices) {
			if(info.type == type ||
			   (info.type == DEVICE_MULTI && info.multi_devices[0].type == type))
			{
				CCLDeviceInfo cinfo = {info.id.c_str(), info.description.c_str(), i++};
				device_list.push_back(cinfo);
			}
		}

		/* null terminate */
		if(!device_list.empty()) {
			CCLDeviceInfo cinfo = {NULL, NULL, 0};
			device_list.push_back(cinfo);
		}
	}

	return (device_list.empty())? NULL: &device_list[0];
}


CCL_NAMESPACE_END

void *CCL_python_module_init()
{
	PyObject *mod = PyModule_Create(&ccl::module);

#ifdef WITH_OSL
	PyModule_AddObject(mod, "with_osl", Py_True);
	Py_INCREF(Py_True);
#else
	PyModule_AddObject(mod, "with_osl", Py_False);
	Py_INCREF(Py_False);
#endif

	return (void*)mod;
}

CCLDeviceInfo *CCL_compute_device_list(int opencl)
{
	ccl::DeviceType type = (opencl)? ccl::DEVICE_OPENCL: ccl::DEVICE_CUDA;
	return ccl::compute_device_list(type);
}

