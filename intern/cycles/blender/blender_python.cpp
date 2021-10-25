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
 * limitations under the License.
 */

#include <Python.h>

#include "blender/CCL_api.h"

#include "blender/blender_sync.h"
#include "blender/blender_session.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_md5.h"
#include "util/util_opengl.h"
#include "util/util_path.h"
#include "util/util_string.h"
#include "util/util_types.h"

#ifdef WITH_OSL
#include "render/osl.h"

#include <OSL/oslquery.h>
#include <OSL/oslconfig.h>
#endif

CCL_NAMESPACE_BEGIN

namespace {

/* Flag describing whether debug flags were synchronized from scene. */
bool debug_flags_set = false;

void *pylong_as_voidptr_typesafe(PyObject *object)
{
	if(object == Py_None)
		return NULL;
	return PyLong_AsVoidPtr(object);
}

/* Synchronize debug flags from a given Blender scene.
 * Return truth when device list needs invalidation.
 */
bool debug_flags_sync_from_scene(BL::Scene b_scene)
{
	DebugFlagsRef flags = DebugFlags();
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	/* Backup some settings for comparison. */
	DebugFlags::OpenCL::DeviceType opencl_device_type = flags.opencl.device_type;
	DebugFlags::OpenCL::KernelType opencl_kernel_type = flags.opencl.kernel_type;
	/* Synchronize CPU flags. */
	flags.cpu.avx2 = get_boolean(cscene, "debug_use_cpu_avx2");
	flags.cpu.avx = get_boolean(cscene, "debug_use_cpu_avx");
	flags.cpu.sse41 = get_boolean(cscene, "debug_use_cpu_sse41");
	flags.cpu.sse3 = get_boolean(cscene, "debug_use_cpu_sse3");
	flags.cpu.sse2 = get_boolean(cscene, "debug_use_cpu_sse2");
	flags.cpu.qbvh = get_boolean(cscene, "debug_use_qbvh");
	flags.cpu.split_kernel = get_boolean(cscene, "debug_use_cpu_split_kernel");
	/* Synchronize CUDA flags. */
	flags.cuda.adaptive_compile = get_boolean(cscene, "debug_use_cuda_adaptive_compile");
	flags.cuda.split_kernel = get_boolean(cscene, "debug_use_cuda_split_kernel");
	/* Synchronize OpenCL kernel type. */
	switch(get_enum(cscene, "debug_opencl_kernel_type")) {
		case 0:
			flags.opencl.kernel_type = DebugFlags::OpenCL::KERNEL_DEFAULT;
			break;
		case 1:
			flags.opencl.kernel_type = DebugFlags::OpenCL::KERNEL_MEGA;
			break;
		case 2:
			flags.opencl.kernel_type = DebugFlags::OpenCL::KERNEL_SPLIT;
			break;
	}
	/* Synchronize OpenCL device type. */
	switch(get_enum(cscene, "debug_opencl_device_type")) {
		case 0:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_NONE;
			break;
		case 1:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_ALL;
			break;
		case 2:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_DEFAULT;
			break;
		case 3:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_CPU;
			break;
		case 4:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_GPU;
			break;
		case 5:
			flags.opencl.device_type = DebugFlags::OpenCL::DEVICE_ACCELERATOR;
			break;
	}
	/* Synchronize other OpenCL flags. */
	flags.opencl.debug = get_boolean(cscene, "debug_use_opencl_debug");
	flags.opencl.mem_limit = ((size_t)get_int(cscene, "debug_opencl_mem_limit"))*1024*1024;
	flags.opencl.single_program = get_boolean(cscene, "debug_opencl_kernel_single_program");
	return flags.opencl.device_type != opencl_device_type ||
	       flags.opencl.kernel_type != opencl_kernel_type;
}

/* Reset debug flags to default values.
 * Return truth when device list needs invalidation.
 */
bool debug_flags_reset()
{
	DebugFlagsRef flags = DebugFlags();
	/* Backup some settings for comparison. */
	DebugFlags::OpenCL::DeviceType opencl_device_type = flags.opencl.device_type;
	DebugFlags::OpenCL::KernelType opencl_kernel_type = flags.opencl.kernel_type;
	flags.reset();
	return flags.opencl.device_type != opencl_device_type ||
	       flags.opencl.kernel_type != opencl_kernel_type;
}

}  /* namespace */

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
	const char *result = _PyUnicode_AsString(py_str);
	if(result) {
		/* 99% of the time this is enough but we better support non unicode
		 * chars since blender doesnt limit this.
		 */
		return result;
	}
	else {
		PyErr_Clear();
		if(PyBytes_Check(py_str)) {
			return PyBytes_AS_STRING(py_str);
		}
		else if((*coerce = PyUnicode_EncodeFSDefault(py_str))) {
			return PyBytes_AS_STRING(*coerce);
		}
		else {
			/* Clear the error, so Cycles can be at leadt used without
			 * GPU and OSL support,
			 */
			PyErr_Clear();
			return "";
		}
	}
}

static PyObject *init_func(PyObject * /*self*/, PyObject *args)
{
	PyObject *path, *user_path;
	int headless;

	if(!PyArg_ParseTuple(args, "OOi", &path, &user_path, &headless)) {
		return NULL;
	}

	PyObject *path_coerce = NULL, *user_path_coerce = NULL;
	path_init(PyC_UnicodeAsByte(path, &path_coerce),
	          PyC_UnicodeAsByte(user_path, &user_path_coerce));
	Py_XDECREF(path_coerce);
	Py_XDECREF(user_path_coerce);

	BlenderSession::headless = headless;

	VLOG(2) << "Debug flags initialized to:\n"
	        << DebugFlags();

	Py_RETURN_NONE;
}


static PyObject *exit_func(PyObject * /*self*/, PyObject * /*args*/)
{
	ShaderManager::free_memory();
	TaskScheduler::free_memory();
	Device::free_memory();
	Py_RETURN_NONE;
}

static PyObject *create_func(PyObject * /*self*/, PyObject *args)
{
	PyObject *pyengine, *pyuserpref, *pydata, *pyscene, *pyregion, *pyv3d, *pyrv3d;
	int preview_osl;

	if(!PyArg_ParseTuple(args, "OOOOOOOi", &pyengine, &pyuserpref, &pydata, &pyscene,
	                     &pyregion, &pyv3d, &pyrv3d, &preview_osl))
	{
		return NULL;
	}

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
	RNA_pointer_create(NULL, &RNA_Region, pylong_as_voidptr_typesafe(pyregion), &regionptr);
	BL::Region region(regionptr);

	PointerRNA v3dptr;
	RNA_pointer_create(NULL, &RNA_SpaceView3D, pylong_as_voidptr_typesafe(pyv3d), &v3dptr);
	BL::SpaceView3D v3d(v3dptr);

	PointerRNA rv3dptr;
	RNA_pointer_create(NULL, &RNA_RegionView3D, pylong_as_voidptr_typesafe(pyrv3d), &rv3dptr);
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

static PyObject *free_func(PyObject * /*self*/, PyObject *value)
{
	delete (BlenderSession*)PyLong_AsVoidPtr(value);

	Py_RETURN_NONE;
}

static PyObject *render_func(PyObject * /*self*/, PyObject *value)
{
	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);

	python_thread_state_save(&session->python_thread_state);

	session->render();

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

/* pixel_array and result passed as pointers */
static PyObject *bake_func(PyObject * /*self*/, PyObject *args)
{
	PyObject *pysession, *pyobject;
	PyObject *pypixel_array, *pyresult;
	const char *pass_type;
	int num_pixels, depth, object_id, pass_filter;

	if(!PyArg_ParseTuple(args, "OOsiiOiiO", &pysession, &pyobject, &pass_type, &pass_filter, &object_id, &pypixel_array, &num_pixels, &depth, &pyresult))
		return NULL;

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);

	PointerRNA objectptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyobject), &objectptr);
	BL::Object b_object(objectptr);

	void *b_result = PyLong_AsVoidPtr(pyresult);

	PointerRNA bakepixelptr;
	RNA_pointer_create(NULL, &RNA_BakePixel, PyLong_AsVoidPtr(pypixel_array), &bakepixelptr);
	BL::BakePixel b_bake_pixel(bakepixelptr);

	python_thread_state_save(&session->python_thread_state);

	session->bake(b_object, pass_type, pass_filter, object_id, b_bake_pixel, (size_t)num_pixels, depth, (float *)b_result);

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

static PyObject *draw_func(PyObject * /*self*/, PyObject *args)
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

static PyObject *reset_func(PyObject * /*self*/, PyObject *args)
{
	PyObject *pysession, *pydata, *pyscene;

	if(!PyArg_ParseTuple(args, "OOO", &pysession, &pydata, &pyscene))
		return NULL;

	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(pysession);

	PointerRNA dataptr;
	RNA_main_pointer_create((Main*)PyLong_AsVoidPtr(pydata), &dataptr);
	BL::BlendData b_data(dataptr);

	PointerRNA sceneptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyscene), &sceneptr);
	BL::Scene b_scene(sceneptr);

	python_thread_state_save(&session->python_thread_state);

	session->reset_session(b_data, b_scene);

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

static PyObject *sync_func(PyObject * /*self*/, PyObject *value)
{
	BlenderSession *session = (BlenderSession*)PyLong_AsVoidPtr(value);

	python_thread_state_save(&session->python_thread_state);

	session->synchronize();

	python_thread_state_restore(&session->python_thread_state);

	Py_RETURN_NONE;
}

static PyObject *available_devices_func(PyObject * /*self*/, PyObject * /*args*/)
{
	vector<DeviceInfo>& devices = Device::available_devices();
	PyObject *ret = PyTuple_New(devices.size());

	for(size_t i = 0; i < devices.size(); i++) {
		DeviceInfo& device = devices[i];
		string type_name = Device::string_from_type(device.type);
		PyObject *device_tuple = PyTuple_New(3);
		PyTuple_SET_ITEM(device_tuple, 0, PyUnicode_FromString(device.description.c_str()));
		PyTuple_SET_ITEM(device_tuple, 1, PyUnicode_FromString(type_name.c_str()));
		PyTuple_SET_ITEM(device_tuple, 2, PyUnicode_FromString(device.id.c_str()));
		PyTuple_SET_ITEM(ret, i, device_tuple);
	}

	return ret;
}

#ifdef WITH_OSL

static PyObject *osl_update_node_func(PyObject * /*self*/, PyObject *args)
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
		string socket_type;
		BL::NodeSocket::type_enum data_type = BL::NodeSocket::type_VALUE;
		float4 default_float4 = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
		float default_float = 0.0f;
		int default_int = 0;
		string default_string = "";

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
		if(param->isoutput) {
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

		for(b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
			if(used_sockets.find(b_input->ptr.data) == used_sockets.end()) {
				b_node.inputs.remove(*b_input);
				removed = true;
				break;
			}
		}

		for(b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if(used_sockets.find(b_output->ptr.data) == used_sockets.end()) {
				b_node.outputs.remove(*b_output);
				removed = true;
				break;
			}
		}
	} while(removed);

	Py_RETURN_TRUE;
}

static PyObject *osl_compile_func(PyObject * /*self*/, PyObject *args)
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

static PyObject *system_info_func(PyObject * /*self*/, PyObject * /*value*/)
{
	string system_info = Device::device_capabilities();
	return PyUnicode_FromString(system_info.c_str());
}

#ifdef WITH_OPENCL
static PyObject *opencl_disable_func(PyObject * /*self*/, PyObject * /*value*/)
{
	VLOG(2) << "Disabling OpenCL platform.";
	DebugFlags().opencl.device_type = DebugFlags::OpenCL::DEVICE_NONE;
	Py_RETURN_NONE;
}
#endif

static PyObject *debug_flags_update_func(PyObject * /*self*/, PyObject *args)
{
	PyObject *pyscene;
	if(!PyArg_ParseTuple(args, "O", &pyscene)) {
		return NULL;
	}

	PointerRNA sceneptr;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyscene), &sceneptr);
	BL::Scene b_scene(sceneptr);

	if(debug_flags_sync_from_scene(b_scene)) {
		VLOG(2) << "Tagging device list for update.";
		Device::tag_update();
	}

	VLOG(2) << "Debug flags set to:\n"
	        << DebugFlags();

	debug_flags_set = true;

	Py_RETURN_NONE;
}

static PyObject *debug_flags_reset_func(PyObject * /*self*/, PyObject * /*args*/)
{
	if(debug_flags_reset()) {
		VLOG(2) << "Tagging device list for update.";
		Device::tag_update();
	}
	if(debug_flags_set) {
		VLOG(2) << "Debug flags reset to:\n"
		        << DebugFlags();
		debug_flags_set = false;
	}
	Py_RETURN_NONE;
}

static PyObject *set_resumable_chunk_func(PyObject * /*self*/, PyObject *args)
{
	int num_resumable_chunks, current_resumable_chunk;
	if(!PyArg_ParseTuple(args, "ii",
	                     &num_resumable_chunks,
	                     &current_resumable_chunk)) {
		Py_RETURN_NONE;
	}

	if(num_resumable_chunks <= 0) {
		fprintf(stderr, "Cycles: Bad value for number of resumable chunks.\n");
		abort();
		Py_RETURN_NONE;
	}
	if(current_resumable_chunk < 1 ||
	   current_resumable_chunk > num_resumable_chunks)
	{
		fprintf(stderr, "Cycles: Bad value for current resumable chunk number.\n");
		abort();
		Py_RETURN_NONE;
	}

	VLOG(1) << "Initialized resumable render: "
	        << "num_resumable_chunks=" << num_resumable_chunks << ", "
	        << "current_resumable_chunk=" << current_resumable_chunk;
	BlenderSession::num_resumable_chunks = num_resumable_chunks;
	BlenderSession::current_resumable_chunk = current_resumable_chunk;

	printf("Cycles: Will render chunk %d of %d\n",
	       current_resumable_chunk,
	       num_resumable_chunks);

	Py_RETURN_NONE;
}

static PyObject *set_resumable_chunk_range_func(PyObject * /*self*/, PyObject *args)
{
	int num_chunks, start_chunk, end_chunk;
	if(!PyArg_ParseTuple(args, "iii",
	                     &num_chunks,
	                     &start_chunk,
	                     &end_chunk)) {
		Py_RETURN_NONE;
	}

	if(num_chunks <= 0) {
		fprintf(stderr, "Cycles: Bad value for number of resumable chunks.\n");
		abort();
		Py_RETURN_NONE;
	}
	if(start_chunk < 1 || start_chunk > num_chunks) {
		fprintf(stderr, "Cycles: Bad value for start chunk number.\n");
		abort();
		Py_RETURN_NONE;
	}
	if(end_chunk < 1 || end_chunk > num_chunks) {
		fprintf(stderr, "Cycles: Bad value for start chunk number.\n");
		abort();
		Py_RETURN_NONE;
	}
	if(start_chunk > end_chunk) {
		fprintf(stderr, "Cycles: End chunk should be higher than start one.\n");
		abort();
		Py_RETURN_NONE;
	}

	VLOG(1) << "Initialized resumable render: "
	        << "num_resumable_chunks=" << num_chunks << ", "
	        << "start_resumable_chunk=" << start_chunk
	        << "end_resumable_chunk=" << end_chunk;
	BlenderSession::num_resumable_chunks = num_chunks;
	BlenderSession::start_resumable_chunk = start_chunk;
	BlenderSession::end_resumable_chunk = end_chunk;

	printf("Cycles: Will render chunks %d to %d of %d\n",
	       start_chunk,
	       end_chunk,
	       num_chunks);

	Py_RETURN_NONE;
}

static PyObject *get_device_types_func(PyObject * /*self*/, PyObject * /*args*/)
{
	vector<DeviceInfo>& devices = Device::available_devices();
	bool has_cuda = false, has_opencl = false;
	for(int i = 0; i < devices.size(); i++) {
		has_cuda   |= (devices[i].type == DEVICE_CUDA);
		has_opencl |= (devices[i].type == DEVICE_OPENCL);
	}
	PyObject *list = PyTuple_New(2);
	PyTuple_SET_ITEM(list, 0, PyBool_FromLong(has_cuda));
	PyTuple_SET_ITEM(list, 1, PyBool_FromLong(has_opencl));
	return list;
}

static PyMethodDef methods[] = {
	{"init", init_func, METH_VARARGS, ""},
	{"exit", exit_func, METH_VARARGS, ""},
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
	{"system_info", system_info_func, METH_NOARGS, ""},
#ifdef WITH_OPENCL
	{"opencl_disable", opencl_disable_func, METH_NOARGS, ""},
#endif

	/* Debugging routines */
	{"debug_flags_update", debug_flags_update_func, METH_VARARGS, ""},
	{"debug_flags_reset", debug_flags_reset_func, METH_NOARGS, ""},

	/* Resumable render */
	{"set_resumable_chunk", set_resumable_chunk_func, METH_VARARGS, ""},
	{"set_resumable_chunk_range", set_resumable_chunk_range_func, METH_VARARGS, ""},

	/* Compute Device selection */
	{"get_device_types", get_device_types_func, METH_VARARGS, ""},

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

#ifdef WITH_CYCLES_DEBUG
	PyModule_AddObject(mod, "with_cycles_debug", Py_True);
	Py_INCREF(Py_True);
#else
	PyModule_AddObject(mod, "with_cycles_debug", Py_False);
	Py_INCREF(Py_False);
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
