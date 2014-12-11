/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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

static void *pylong_as_voidptr_typesafe(PyObject *object)
{
	if(object == Py_None)
		return NULL;
	return PyLong_AsVoidPtr(object);
}

void python_thread_state_save(void **python_thread_state)
{
	*python_thread_state = (void*)PyEval_SaveThread();
}

void python_thread_state_restore(void **python_thread_state)
{
	PyEval_RestoreThread((PyThreadState*)*python_thread_state);
	*python_thread_state = NULL;
}

static const char *PyC_UnicodeAsByte(PyObject *py_str, PyObject **coerce)
{
#ifdef WIN32
	/* bug [#31856] oddly enough, Python3.2 --> 3.3 on Windows will throw an
	 * exception here this needs to be fixed in python:
	 * see: bugs.python.org/issue15859 */
	if(!PyUnicode_Check(py_str)) {
		PyErr_BadArgument();
		return "";
	}
#endif
	if((*coerce = PyUnicode_EncodeFSDefault(py_str))) {
		return PyBytes_AS_STRING(*coerce);
	}
	return "";
}

static PyObject *init_func(PyObject *self, PyObject *args)
{
	PyObject *path, *user_path;

	if(!PyArg_ParseTuple(args, "OO", &path, &user_path)) {
		return NULL;
	}

	PyObject *path_coerce = NULL, *user_path_coerce = NULL;
	path_init(PyC_UnicodeAsByte(path, &path_coerce),
	          PyC_UnicodeAsByte(user_path, &user_path_coerce));
	Py_XDECREF(path_coerce);
	Py_XDECREF(user_path_coerce);

	Py_RETURN_NONE;
}

static PyObject *create_func(PyObject *self, PyObject *args)
{
	PyObject *pyengine, *pyuserpref, *pydata, *pyscene, *pyregion, *pyv3d, *pyrv3d;
	int preview_osl;

	if(!PyArg_ParseTuple(args, "OOOOOOOi", &pyengine, &pyuserpref, &pydata, &pyscene, &pyregion, &pyv3d, &pyrv3d, &preview_osl))
		return NULL;

	/* RNA */
	PointerRNA engineptr;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(pyengine), &engineptr);
	BL::RenderEngine engine(engineptr);

	PointerRNA userprefptr;
	RNA_pointer_create(NULL, &RNA_UserPreferences, (void*)PyLong_AsVoidPtr(pyuserpref), &userprefptr);
	BL::UserPreferences userpref(userprefptr);

	PointerRNA dataptr;
	RNA_main_pointer_create((Main*)PyLong_AsVoidPtr(pydata), &dataptr);
	BL::BlendData data(dataptr);

	PointerRNA sceneptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyscene), &sceneptr);
	BL::Scene scene(sceneptr);

	PointerRNA regionptr;
	RNA_id_pointer_create((ID*)pylong_as_voidptr_typesafe(pyregion), &regionptr);
	BL::Region region(regionptr);

	PointerRNA v3dptr;
	RNA_id_pointer_create((ID*)pylong_as_voidptr_typesafe(pyv3d), &v3dptr);
	BL::SpaceView3D v3d(v3dptr);

	PointerRNA rv3dptr;
	RNA_id_pointer_create((ID*)pylong_as_voidptr_typesafe(pyrv3d), &rv3dptr);
	BL::RegionView3D rv3d(rv3dptr);

	/* create session */
	BlenderSession *session;

	if(rv3d) {
		/* interactive viewport session */
		int width = region.width();
		int height = region.height();

		session = new BlenderSession(engine, userpref, data, scene, v3d, rv3d, width, height);
	}
	else {
		/* override some settings for preview */
		if(engine.is_preview()) {
			PointerRNA cscene = RNA_pointer_get(&sceneptr, "cycles");

			RNA_boolean_set(&cscene, "shading_system", preview_osl);
			RNA_boolean_set(&cscene, "use_progressive_refine", true);
		}

		/* offline session or preview render */
		session = new BlenderSession(engine, userpref, data, scene);
	}

	python_thread_state_save(&session->python_thread_state);

	session->create();

	python_thread_state_restore(&session->python_thread_state);

	return PyLong_FromVoidPtr(session);
}

static PyObject *free_func(PyObject *self, PyObject *value)
{
	delete (BlenderSession*)PyLong_AsVoidPtr(value);

	Py_RETURN_NONE;
}

static PyObject *render_func(PyObject *self, PyObject *value)
{
	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);

	python_thread_state_save(&session->python_thread_state);

	session->render();

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

/* pixel_array and result passed as pointers */
static PyObject *bake_func(PyObject *self, PyObject *args)
{
	PyObject *pysession, *pyobject;
	PyObject *pypixel_array, *pyresult;
	const char *pass_type;
	int num_pixels, depth;

	if(!PyArg_ParseTuple(args, "OOsOiiO", &pysession, &pyobject, &pass_type, &pypixel_array,  &num_pixels, &depth, &pyresult))
		return NULL;

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);

	PointerRNA objectptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyobject), &objectptr);
	BL::Object b_object(objectptr);

	void *b_result = PyLong_AsVoidPtr(pyresult);

	PointerRNA bakepixelptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pypixel_array), &bakepixelptr);
	BL::BakePixel b_bake_pixel(bakepixelptr);

	python_thread_state_save(&session->python_thread_state);

	session->bake(b_object, pass_type, b_bake_pixel, (size_t)num_pixels, depth, (float *)b_result);

	python_thread_state_restore(&session->python_thread_state);

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

static PyObject *reset_func(PyObject *self, PyObject *args)
{
	PyObject *pysession, *pydata, *pyscene;

	if(!PyArg_ParseTuple(args, "OOO", &pysession, &pydata, &pyscene))
		return NULL;

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);

	PointerRNA dataptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pydata), &dataptr);
	BL::BlendData b_data(dataptr);

	PointerRNA sceneptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyscene), &sceneptr);
	BL::Scene b_scene(sceneptr);

	python_thread_state_save(&session->python_thread_state);

	session->reset_session(b_data, b_scene);

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

static PyObject *sync_func(PyObject *self, PyObject *value)
{
	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);

	python_thread_state_save(&session->python_thread_state);

	session->synchronize();

	python_thread_state_restore(&session->python_thread_state);

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
		std::string socket_type;
		BL::NodeSocket::type_enum data_type = BL::NodeSocket::type_VALUE;
		float4 default_float4 = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
		float default_float = 0.0f;
		int default_int = 0;
		std::string default_string = "";
		
		if(param->isclosure) {
			socket_type = "NodeSocketShader";
			data_type = BL::NodeSocket::type_SHADER;
		}
		else if(param->type.vecsemantics == TypeDesc::COLOR) {
			socket_type = "NodeSocketColor";
			data_type = BL::NodeSocket::type_RGBA;

			if(param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		}
		else if(param->type.vecsemantics == TypeDesc::POINT ||
		        param->type.vecsemantics == TypeDesc::VECTOR ||
		        param->type.vecsemantics == TypeDesc::NORMAL)
		{
			socket_type = "NodeSocketVector";
			data_type = BL::NodeSocket::type_VECTOR;

			if(param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		}
		else if(param->type.aggregate == TypeDesc::SCALAR) {
			if(param->type.basetype == TypeDesc::INT) {
				socket_type = "NodeSocketInt";
				data_type = BL::NodeSocket::type_INT;
				if(param->validdefault)
					default_int = param->idefault[0];
			}
			else if(param->type.basetype == TypeDesc::FLOAT) {
				socket_type = "NodeSocketFloat";
				data_type = BL::NodeSocket::type_VALUE;
				if(param->validdefault)
					default_float = param->fdefault[0];
			}
			else if(param->type.basetype == TypeDesc::STRING) {
				socket_type = "NodeSocketString";
				data_type = BL::NodeSocket::type_STRING;
				if(param->validdefault)
					default_string = param->sdefault[0];
			}
			else
				continue;
		}
		else
			continue;

		/* find socket socket */
		BL::NodeSocket b_sock(PointerRNA_NULL);
		if (param->isoutput) {
			b_sock = b_node.outputs[param->name.string()];
			/* remove if type no longer matches */
			if(b_sock && b_sock.bl_idname() != socket_type) {
				b_node.outputs.remove(b_sock);
				b_sock = BL::NodeSocket(PointerRNA_NULL);
			}
		}
		else {
			b_sock = b_node.inputs[param->name.string()];
			/* remove if type no longer matches */
			if(b_sock && b_sock.bl_idname() != socket_type) {
				b_node.inputs.remove(b_sock);
				b_sock = BL::NodeSocket(PointerRNA_NULL);
			}
		}

		if(!b_sock) {
			/* create new socket */
			if(param->isoutput)
				b_sock = b_node.outputs.create(socket_type.c_str(), param->name.c_str(), param->name.c_str());
			else
				b_sock = b_node.inputs.create(socket_type.c_str(), param->name.c_str(), param->name.c_str());

			/* set default value */
			if(data_type == BL::NodeSocket::type_VALUE) {
				set_float(b_sock.ptr, "default_value", default_float);
			}
			else if(data_type == BL::NodeSocket::type_INT) {
				set_int(b_sock.ptr, "default_value", default_int);
			}
			else if(data_type == BL::NodeSocket::type_RGBA) {
				set_float4(b_sock.ptr, "default_value", default_float4);
			}
			else if(data_type == BL::NodeSocket::type_VECTOR) {
				set_float3(b_sock.ptr, "default_value", float4_to_float3(default_float4));
			}
			else if(data_type == BL::NodeSocket::type_STRING) {
				set_string(b_sock.ptr, "default_value", default_string);
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
				b_node.inputs.remove(*b_input);
				removed = true;
				break;
			}
		}

		for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if(used_sockets.find(b_output->ptr.data) == used_sockets.end()) {
				b_node.outputs.remove(*b_output);
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
	{"bake", bake_func, METH_VARARGS, ""},
	{"draw", draw_func, METH_VARARGS, ""},
	{"sync", sync_func, METH_O, ""},
	{"reset", reset_func, METH_VARARGS, ""},
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
				CCLDeviceInfo cinfo;

				strncpy(cinfo.identifier, info.id.c_str(), sizeof(cinfo.identifier));
				cinfo.identifier[info.id.length()] = '\0';

				strncpy(cinfo.name, info.description.c_str(), sizeof(cinfo.name));
				cinfo.name[info.description.length()] = '\0';

				cinfo.value = i++;

				device_list.push_back(cinfo);
			}
		}

		/* null terminate */
		if(!device_list.empty()) {
			CCLDeviceInfo cinfo = {"", "", 0};
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
	/* TODO(sergey): This gives us library we've been linking against.
	 *               In theory with dynamic OSL library it might not be
	 *               accurate, but there's nothing in OSL API which we
	 *               might use to get version in runtime.
	 */
	int curversion = OSL_LIBRARY_VERSION_CODE;
	PyModule_AddObject(mod, "with_osl", Py_True);
	Py_INCREF(Py_True);
	PyModule_AddObject(mod, "osl_version",
	                   Py_BuildValue("(iii)",
	                                  curversion / 10000, (curversion / 100) % 100, curversion % 100));
	PyModule_AddObject(mod, "osl_version_string",
	                   PyUnicode_FromFormat("%2d, %2d, %2d",
	                                        curversion / 10000, (curversion / 100) % 100, curversion % 100));
#else
	PyModule_AddObject(mod, "with_osl", Py_False);
	Py_INCREF(Py_False);
	PyModule_AddStringConstant(mod, "osl_version", "unknown");
	PyModule_AddStringConstant(mod, "osl_version_string", "unknown");
#endif

#ifdef WITH_NETWORK
	PyModule_AddObject(mod, "with_network", Py_True);
	Py_INCREF(Py_True);
#else /* WITH_NETWORK */
	PyModule_AddObject(mod, "with_network", Py_False);
	Py_INCREF(Py_False);
#endif /* WITH_NETWORK */

	return (void*)mod;
}

CCLDeviceInfo *CCL_compute_device_list(int device_type)
{
	ccl::DeviceType type;
	switch(device_type) {
		case 0:
			type = ccl::DEVICE_CUDA;
			break;
		case 1:
			type = ccl::DEVICE_OPENCL;
			break;
		case 2:
			type = ccl::DEVICE_NETWORK;
			break;
		default:
			type = ccl::DEVICE_NONE;
			break;
	}
	return ccl::compute_device_list(type);
}

