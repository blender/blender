/*
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

/** \file
 * \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_shader.h"
#include "GPU_shader_interface.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"
#include "../mathutils/mathutils.h"

#include "gpu_py_api.h"
#include "gpu_py_shader.h" /* own include */
#include "gpu_py_vertex_format.h"

/* -------------------------------------------------------------------- */

/** \name Enum Conversion.
 * \{ */

static int bpygpu_ParseBultinShaderEnum(PyObject *o, void *p)
{
  Py_ssize_t mode_id_len;
  const char *mode_id = _PyUnicode_AsStringAndSize(o, &mode_id_len);
  if (mode_id == NULL) {
    PyErr_Format(PyExc_ValueError, "expected a string, got %s", Py_TYPE(o)->tp_name);
    return 0;
  }
#define MATCH_ID(id) \
  if (mode_id_len == (Py_ssize_t)strlen(STRINGIFY(id))) { \
    if (STREQ(mode_id, STRINGIFY(id))) { \
      mode = GPU_SHADER_##id; \
      goto success; \
    } \
  } \
  ((void)0)

  eGPUBuiltinShader mode;
  MATCH_ID(2D_UNIFORM_COLOR);
  MATCH_ID(2D_FLAT_COLOR);
  MATCH_ID(2D_SMOOTH_COLOR);
  MATCH_ID(2D_IMAGE);
  MATCH_ID(3D_UNIFORM_COLOR);
  MATCH_ID(3D_FLAT_COLOR);
  MATCH_ID(3D_SMOOTH_COLOR);

#undef MATCH_ID
  PyErr_Format(PyExc_ValueError, "unknown type literal: '%s'", mode_id);
  return 0;

success:
  (*(eGPUBuiltinShader *)p) = mode;
  return 1;
}

static int bpygpu_uniform_location_get(GPUShader *shader,
                                       const char *name,
                                       const char *error_prefix)
{
  int uniform = GPU_shader_get_uniform_ensure(shader, name);

  if (uniform == -1) {
    PyErr_Format(PyExc_ValueError, "%s: uniform %.32s not found", error_prefix, name);
  }

  return uniform;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Type
 * \{ */

static PyObject *bpygpu_shader_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  struct {
    const char *vertexcode;
    const char *fragcode;
    const char *geocode;
    const char *libcode;
    const char *defines;
  } params = {0};

  static const char *_keywords[] = {
      "vertexcode", "fragcode", "geocode", "libcode", "defines", NULL};

  static _PyArg_Parser _parser = {"ss|$sss:GPUShader.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &params.vertexcode,
                                        &params.fragcode,
                                        &params.geocode,
                                        &params.libcode,
                                        &params.defines)) {
    return NULL;
  }

  GPUShader *shader = GPU_shader_create(
      params.vertexcode, params.fragcode, params.geocode, params.libcode, params.defines, NULL);

  if (shader == NULL) {
    PyErr_SetString(PyExc_Exception, "Shader Compile Error, see console for more details");
    return NULL;
  }

  return BPyGPUShader_CreatePyObject(shader, false);
}

PyDoc_STRVAR(
    bpygpu_shader_bind_doc,
    ".. method:: bind()\n"
    "\n"
    "   Bind the shader object. Required to be able to change uniforms of this shader.\n");
static PyObject *bpygpu_shader_bind(BPyGPUShader *self)
{
  GPU_shader_bind(self->shader);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_from_name_doc,
             ".. method:: uniform_from_name(name)\n"
             "\n"
             "   Get uniform location by name.\n"
             "\n"
             "   :param name: Name of the uniform variable whose location is to be queried.\n"
             "   :type name: `str`\n"
             "   :return: Location of the uniform variable.\n"
             "   :rtype: `int`\n");
static PyObject *bpygpu_shader_uniform_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == NULL) {
    return NULL;
  }

  int uniform = bpygpu_uniform_location_get(self->shader, name, "GPUShader.get_uniform");

  if (uniform == -1) {
    return NULL;
  }

  return PyLong_FromLong(uniform);
}

PyDoc_STRVAR(
    bpygpu_shader_uniform_block_from_name_doc,
    ".. method:: uniform_block_from_name(name)\n"
    "\n"
    "   Get uniform block location by name.\n"
    "\n"
    "   :param name: Name of the uniform block variable whose location is to be queried.\n"
    "   :type name: `str`\n"
    "   :return: The location of the uniform block variable.\n"
    "   :rtype: `int`\n");
static PyObject *bpygpu_shader_uniform_block_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == NULL) {
    return NULL;
  }

  int uniform = GPU_shader_get_uniform_block(self->shader, name);

  if (uniform == -1) {
    PyErr_Format(PyExc_ValueError, "GPUShader.get_uniform_block: uniform %.32s not found", name);
    return NULL;
  }

  return PyLong_FromLong(uniform);
}

static bool bpygpu_shader_uniform_vector_imp(PyObject *args,
                                             int elem_size,
                                             int *r_location,
                                             int *r_length,
                                             int *r_count,
                                             Py_buffer *r_pybuffer)
{
  PyObject *buffer;

  *r_count = 1;
  if (!PyArg_ParseTuple(
          args, "iOi|i:GPUShader.uniform_vector_*", r_location, &buffer, r_length, r_count)) {
    return false;
  }

  if (PyObject_GetBuffer(buffer, r_pybuffer, PyBUF_SIMPLE) == -1) {
    /* PyObject_GetBuffer raise a PyExc_BufferError */
    return false;
  }

  if (r_pybuffer->len != (*r_length * *r_count * elem_size)) {
    PyErr_SetString(PyExc_BufferError, "GPUShader.uniform_vector_*: buffer size does not match.");
    return false;
  }

  return true;
}

PyDoc_STRVAR(bpygpu_shader_uniform_vector_float_doc,
             ".. method:: uniform_vector_float(location, buffer, length, count)\n"
             "\n"
             "   Set the buffer to fill the uniform.\n"
             "\n"
             "   :param location: Location of the uniform variable to be modified.\n"
             "   :type location: int\n"
             "   :param buffer:  The data that should be set. Can support the buffer protocol.\n"
             "   :type buffer: sequence of floats\n"
             "   :param length: Size of the uniform data type:\n"
             "\n"
             "      - 1: float\n"
             "      - 2: vec2 or float[2]\n"
             "      - 3: vec3 or float[3]\n"
             "      - 4: vec4 or float[4]\n"
             "      - 9: mat3\n"
             "      - 16: mat4\n"
             "\n"
             "   :type length: int\n"
             "   :param count: Specifies the number of elements, vector or matrices that are to "
             "be modified.\n"
             "   :type count: int\n");
static PyObject *bpygpu_shader_uniform_vector_float(BPyGPUShader *self, PyObject *args)
{
  int location, length, count;

  Py_buffer pybuffer;

  if (!bpygpu_shader_uniform_vector_imp(
          args, sizeof(float), &location, &length, &count, &pybuffer)) {
    return NULL;
  }

  GPU_shader_uniform_vector(self->shader, location, length, count, pybuffer.buf);

  PyBuffer_Release(&pybuffer);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_vector_int_doc,
             ".. method:: uniform_vector_int(location, buffer, length, count)\n"
             "\n"
             "   See GPUShader.uniform_vector_float(...) description.\n");
static PyObject *bpygpu_shader_uniform_vector_int(BPyGPUShader *self, PyObject *args)
{
  int location, length, count;

  Py_buffer pybuffer;

  if (!bpygpu_shader_uniform_vector_imp(
          args, sizeof(int), &location, &length, &count, &pybuffer)) {
    return NULL;
  }

  GPU_shader_uniform_vector_int(self->shader, location, length, count, pybuffer.buf);

  PyBuffer_Release(&pybuffer);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_bool_doc,
             ".. method:: uniform_bool(name, seq)\n"
             "\n"
             "   Specify the value of a uniform variable for the current program object.\n"
             "\n"
             "   :param name: Name of the uniform variable whose value is to be changed.\n"
             "   :type name: str\n"
             "   :param seq: Value that will be used to update the specified uniform variable.\n"
             "   :type seq: sequence of bools\n");
static PyObject *bpygpu_shader_uniform_bool(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_bool";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_bool", &params.id, &params.seq)) {
    return NULL;
  }

  int values[4];
  int length;
  int ret;
  {
    PyObject *seq_fast = PySequence_Fast(params.seq, error_prefix);
    if (seq_fast == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%s: expected a sequence, got %s",
                   error_prefix,
                   Py_TYPE(params.seq)->tp_name);
      ret = -1;
    }
    else {
      length = PySequence_Fast_GET_SIZE(seq_fast);
      if (length == 0 || length > 4) {
        PyErr_Format(PyExc_TypeError,
                     "%s: invalid sequence length. expected 1..4, got %d",
                     error_prefix,
                     length);
        ret = -1;
      }
      else {
        ret = PyC_AsArray_FAST(values, seq_fast, length, &PyLong_Type, false, error_prefix);
      }
      Py_DECREF(seq_fast);
    }
  }
  if (ret == -1) {
    return NULL;
  }

  const int location = bpygpu_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return NULL;
  }

  GPU_shader_uniform_vector_int(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_float_doc,
             ".. method:: uniform_float(name, value)\n"
             "\n"
             "   Specify the value of a uniform variable for the current program object.\n"
             "\n"
             "   :param name: Name of the uniform variable whose value is to be changed.\n"
             "   :type name: str\n"
             "   :param value: Value that will be used to update the specified uniform variable.\n"
             "   :type value: single number or sequence of numbers\n");
static PyObject *bpygpu_shader_uniform_float(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_float";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_float", &params.id, &params.seq)) {
    return NULL;
  }

  float values[16];
  int length;

  if (PyFloat_Check(params.seq)) {
    values[0] = (float)PyFloat_AsDouble(params.seq);
    length = 1;
  }
  else if (PyLong_Check(params.seq)) {
    values[0] = (float)PyLong_AsDouble(params.seq);
    length = 1;
  }
  else if (MatrixObject_Check(params.seq)) {
    MatrixObject *mat = (MatrixObject *)params.seq;
    if (BaseMath_ReadCallback(mat) == -1) {
      return NULL;
    }
    if ((mat->num_row != mat->num_col) || !ELEM(mat->num_row, 3, 4)) {
      PyErr_SetString(PyExc_ValueError, "Expected 3x3 or 4x4 matrix");
      return NULL;
    }
    length = mat->num_row * mat->num_col;
    memcpy(values, mat->matrix, sizeof(float) * length);
  }
  else {
    length = mathutils_array_parse(values, 2, 16, params.seq, "");
    if (length == -1) {
      return NULL;
    }
  }

  if (!ELEM(length, 1, 2, 3, 4, 9, 16)) {
    PyErr_SetString(PyExc_TypeError,
                    "Expected a single float or a sequence of floats of length 1..4, 9 or 16.");
    return NULL;
  }

  const int location = bpygpu_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return NULL;
  }

  GPU_shader_uniform_vector(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_int_doc,
             ".. method:: uniform_int(name, seq)\n"
             "\n"
             "   Specify the value of a uniform variable for the current program object.\n"
             "\n"
             "   :param name: name of the uniform variable whose value is to be changed.\n"
             "   :type name: str\n"
             "   :param seq: Value that will be used to update the specified uniform variable.\n"
             "   :type seq: sequence of numbers\n");
static PyObject *bpygpu_shader_uniform_int(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_int";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_int", &params.id, &params.seq)) {
    return NULL;
  }

  int values[4];
  int length;
  int ret;

  if (PyLong_Check(params.seq)) {
    values[0] = PyC_Long_AsI32(params.seq);
    length = 1;
    ret = 0;
  }
  else {
    PyObject *seq_fast = PySequence_Fast(params.seq, error_prefix);
    if (seq_fast == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%s: expected a sequence, got %s",
                   error_prefix,
                   Py_TYPE(params.seq)->tp_name);
      ret = -1;
    }
    else {
      length = PySequence_Fast_GET_SIZE(seq_fast);
      if (length == 0 || length > 4) {
        PyErr_Format(PyExc_TypeError,
                     "%s: invalid sequence length. expected 1..4, got %d",
                     error_prefix,
                     length);
        ret = -1;
      }
      else {
        ret = PyC_AsArray_FAST(values, seq_fast, length, &PyLong_Type, false, error_prefix);
      }
      Py_DECREF(seq_fast);
    }
  }
  if (ret == -1) {
    return NULL;
  }

  const int location = bpygpu_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return NULL;
  }

  GPU_shader_uniform_vector_int(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    bpygpu_shader_attr_from_name_doc,
    ".. method:: attr_from_name(name)\n"
    "\n"
    "   Get attribute location by name.\n"
    "\n"
    "   :param name: The name of the attribute variable whose location is to be queried.\n"
    "   :type name: str\n"
    "   :return: The location of an attribute variable.\n"
    "   :rtype: int\n");
static PyObject *bpygpu_shader_attr_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == NULL) {
    return NULL;
  }

  int attr = GPU_shader_get_attribute(self->shader, name);

  if (attr == -1) {
    PyErr_Format(PyExc_ValueError, "GPUShader.attr_from_name: attribute %.32s not found", name);
    return NULL;
  }

  return PyLong_FromLong(attr);
}

PyDoc_STRVAR(bpygpu_shader_calc_format_doc,
             ".. method:: calc_format()\n"
             "\n"
             "   Build a new format based on the attributes of the shader.\n"
             "\n"
             "   :return: vertex attribute format for the shader\n"
             "   :rtype: GPUVertFormat\n");
static PyObject *bpygpu_shader_calc_format(BPyGPUShader *self, PyObject *UNUSED(arg))
{
  BPyGPUVertFormat *ret = (BPyGPUVertFormat *)BPyGPUVertFormat_CreatePyObject(NULL);
  GPU_vertformat_from_interface(&ret->fmt, GPU_shader_get_interface(self->shader));
  return (PyObject *)ret;
}

static struct PyMethodDef bpygpu_shader_methods[] = {
    {"bind", (PyCFunction)bpygpu_shader_bind, METH_NOARGS, bpygpu_shader_bind_doc},
    {"uniform_from_name",
     (PyCFunction)bpygpu_shader_uniform_from_name,
     METH_O,
     bpygpu_shader_uniform_from_name_doc},
    {"uniform_block_from_name",
     (PyCFunction)bpygpu_shader_uniform_block_from_name,
     METH_O,
     bpygpu_shader_uniform_block_from_name_doc},
    {"uniform_vector_float",
     (PyCFunction)bpygpu_shader_uniform_vector_float,
     METH_VARARGS,
     bpygpu_shader_uniform_vector_float_doc},
    {"uniform_vector_int",
     (PyCFunction)bpygpu_shader_uniform_vector_int,
     METH_VARARGS,
     bpygpu_shader_uniform_vector_int_doc},
    {"uniform_bool",
     (PyCFunction)bpygpu_shader_uniform_bool,
     METH_VARARGS,
     bpygpu_shader_uniform_bool_doc},
    {"uniform_float",
     (PyCFunction)bpygpu_shader_uniform_float,
     METH_VARARGS,
     bpygpu_shader_uniform_float_doc},
    {"uniform_int",
     (PyCFunction)bpygpu_shader_uniform_int,
     METH_VARARGS,
     bpygpu_shader_uniform_int_doc},
    {"attr_from_name",
     (PyCFunction)bpygpu_shader_attr_from_name,
     METH_O,
     bpygpu_shader_attr_from_name_doc},
    {"format_calc",
     (PyCFunction)bpygpu_shader_calc_format,
     METH_NOARGS,
     bpygpu_shader_calc_format_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(
    bpygpu_shader_program_doc,
    "The name of the program object for use by the OpenGL API (read-only).\n\n:type: int");
static PyObject *bpygpu_shader_program_get(BPyGPUShader *self, void *UNUSED(closure))
{
  return PyLong_FromLong(GPU_shader_get_program(self->shader));
}

static PyGetSetDef bpygpu_shader_getseters[] = {
    {(char *)"program",
     (getter)bpygpu_shader_program_get,
     (setter)NULL,
     bpygpu_shader_program_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static void bpygpu_shader_dealloc(BPyGPUShader *self)
{
  if (self->is_builtin == false) {
    GPU_shader_free(self->shader);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(
    bpygpu_shader_doc,
    ".. class:: GPUShader(vertexcode, fragcode, geocode=None, libcode=None, defines=None)\n"
    "\n"
    "   GPUShader combines multiple GLSL shaders into a program used for drawing.\n"
    "   It must contain a vertex and fragment shaders, with an optional geometry shader.\n"
    "\n"
    "   The GLSL #version directive is automatically included at the top of shaders, and set to "
    "330.\n"
    "   Some preprocessor directives are automatically added according to the Operating System or "
    "availability:\n"
    "   ``GPU_ATI``, ``GPU_NVIDIA`` and ``GPU_INTEL``.\n"
    "\n"
    "   The following extensions are enabled by default if supported by the GPU:\n"
    "   ``GL_ARB_texture_gather`` and ``GL_ARB_texture_query_lod``.\n"
    "\n"
    "   To debug shaders, use the --debug-gpu-shaders command line option"
    "   to see full GLSL shader compilation and linking errors.\n"
    "\n"
    "   :param vertexcode: Vertex shader code.\n"
    "   :type vertexcode: str\n"
    "   :param fragcode: Fragment shader code.\n"
    "   :type value: str\n"
    "   :param geocode: Geometry shader code.\n"
    "   :type value: str\n"
    "   :param libcode: Code with functions and presets to be shared between shaders.\n"
    "   :type value: str\n"
    "   :param defines: Preprocessor directives.\n"
    "   :type value: str\n");
PyTypeObject BPyGPUShader_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUShader",
    .tp_basicsize = sizeof(BPyGPUShader),
    .tp_dealloc = (destructor)bpygpu_shader_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = bpygpu_shader_doc,
    .tp_methods = bpygpu_shader_methods,
    .tp_getset = bpygpu_shader_getseters,
    .tp_new = bpygpu_shader_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name gpu.shader Module API
 * \{ */

PyDoc_STRVAR(bpygpu_shader_unbind_doc,
             ".. function:: unbind()\n"
             "\n"
             "   Unbind the bound shader object.\n");
static PyObject *bpygpu_shader_unbind(BPyGPUShader *UNUSED(self))
{
  GPU_shader_unbind();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_from_builtin_doc,
             ".. function:: from_builtin(shader_name)\n"
             "\n"
             "   Shaders that are embedded in the blender internal code.\n"
             "   They all read the uniform 'mat4 ModelViewProjectionMatrix', which can be edited "
             "by the 'gpu.matrix' module.\n"
             "   For more details, you can check the shader code with the function "
             "'gpu.shader.code_from_builtin';\n"
             "\n"
             "   :param shader_name: One of these builtin shader names: {\n"
             "       '2D_UNIFORM_COLOR',\n"
             "       '2D_FLAT_COLOR',\n"
             "       '2D_SMOOTH_COLOR',\n"
             "       '2D_IMAGE',\n"
             "       '3D_UNIFORM_COLOR',\n"
             "       '3D_FLAT_COLOR',\n"
             "       '3D_SMOOTH_COLOR'}\n"
             "   :type shader_name: str\n"
             "   :return: Shader object corresponding to the given name.\n"
             "   :rtype: :class:`bpy.types.GPUShader`\n");
static PyObject *bpygpu_shader_from_builtin(PyObject *UNUSED(self), PyObject *arg)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  eGPUBuiltinShader shader_id;

  if (!bpygpu_ParseBultinShaderEnum(arg, &shader_id)) {
    return NULL;
  }

  GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);

  return BPyGPUShader_CreatePyObject(shader, true);
}

PyDoc_STRVAR(bpygpu_shader_code_from_builtin_doc,
             ".. function:: code_from_builtin(shader_name)\n"
             "\n"
             "   Exposes the internal shader code for query.\n"
             "\n"
             "   :param shader_name: One of these builtin shader names: {\n"
             "       '2D_UNIFORM_COLOR',\n"
             "       '2D_FLAT_COLOR',\n"
             "       '2D_SMOOTH_COLOR',\n"
             "       '2D_IMAGE',\n"
             "       '3D_UNIFORM_COLOR',\n"
             "       '3D_FLAT_COLOR',\n"
             "       '3D_SMOOTH_COLOR'}\n"
             "   :type shader_name: str\n"
             "   :return: Vertex, fragment and geometry shader codes.\n"
             "   :rtype: dict\n");
static PyObject *bpygpu_shader_code_from_builtin(BPyGPUShader *UNUSED(self), PyObject *arg)
{
  eGPUBuiltinShader shader_id;

  const char *vert;
  const char *frag;
  const char *geom;
  const char *defines;

  PyObject *item, *r_dict;

  if (!bpygpu_ParseBultinShaderEnum(arg, &shader_id)) {
    return NULL;
  }

  GPU_shader_get_builtin_shader_code(shader_id, &vert, &frag, &geom, &defines);

  r_dict = PyDict_New();

  PyDict_SetItemString(r_dict, "vertex_shader", item = PyUnicode_FromString(vert));
  Py_DECREF(item);

  PyDict_SetItemString(r_dict, "fragment_shader", item = PyUnicode_FromString(frag));
  Py_DECREF(item);

  if (geom) {
    PyDict_SetItemString(r_dict, "geometry_shader", item = PyUnicode_FromString(geom));
    Py_DECREF(item);
  }
  if (defines) {
    PyDict_SetItemString(r_dict, "defines", item = PyUnicode_FromString(defines));
    Py_DECREF(item);
  }
  return r_dict;
}

static struct PyMethodDef bpygpu_shader_module_methods[] = {
    {"unbind", (PyCFunction)bpygpu_shader_unbind, METH_NOARGS, bpygpu_shader_unbind_doc},
    {"from_builtin",
     (PyCFunction)bpygpu_shader_from_builtin,
     METH_O,
     bpygpu_shader_from_builtin_doc},
    {"code_from_builtin",
     (PyCFunction)bpygpu_shader_code_from_builtin,
     METH_O,
     bpygpu_shader_code_from_builtin_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpygpu_shader_module_doc,
             "This module provides access to GPUShader internal functions.");
static PyModuleDef BPyGPU_shader_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.shader",
    .m_doc = bpygpu_shader_module_doc,
    .m_methods = bpygpu_shader_module_methods,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUShader_CreatePyObject(GPUShader *shader, bool is_builtin)
{
  BPyGPUShader *self;

  self = PyObject_New(BPyGPUShader, &BPyGPUShader_Type);
  self->shader = shader;
  self->is_builtin = is_builtin;

  return (PyObject *)self;
}

PyObject *BPyInit_gpu_shader(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPyGPU_shader_module_def);

  return submodule;
}

/** \} */
