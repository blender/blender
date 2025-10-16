/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_context.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

#include "../mathutils/mathutils.hh"

#include "gpu_py.hh"
#include "gpu_py_texture.hh"
#include "gpu_py_uniformbuffer.hh"
#include "gpu_py_vertex_format.hh"

#include "gpu_py_shader.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Enum Conversion.
 * \{ */

#define PYDOC_BUILTIN_SHADER_DESCRIPTION \
  "``FLAT_COLOR``\n" \
  "   :Attributes: vec3 pos, vec4 color\n" \
  "   :Uniforms: none\n" \
  "``IMAGE``\n" \
  "   :Attributes: vec3 pos, vec2 texCoord\n" \
  "   :Uniforms: sampler2D image\n" \
  "``IMAGE_SCENE_LINEAR_TO_REC709_SRGB``\n" \
  "   :Attributes: vec3 pos, vec2 texCoord\n" \
  "   :Uniforms: sampler2D image\n" \
  "   :Note: Expect texture to be in scene linear color space\n" \
  "``IMAGE_COLOR``\n" \
  "   :Attributes: vec3 pos, vec2 texCoord\n" \
  "   :Uniforms: sampler2D image, vec4 color\n" \
  "``IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB``\n" \
  "   :Attributes: vec3 pos, vec2 texCoord\n" \
  "   :Uniforms: sampler2D image, vec4 color\n" \
  "   :Note: Expect texture to be in scene linear color space\n" \
  "``SMOOTH_COLOR``\n" \
  "   :Attributes: vec3 pos, vec4 color\n" \
  "   :Uniforms: none\n" \
  "``UNIFORM_COLOR``\n" \
  "   :Attributes: vec3 pos\n" \
  "   :Uniforms: vec4 color\n" \
  "``POLYLINE_FLAT_COLOR``\n" \
  "   :Attributes: vec3 pos, vec4 color\n" \
  "   :Uniforms: vec2 viewportSize, float lineWidth\n" \
  "``POLYLINE_SMOOTH_COLOR``\n" \
  "   :Attributes: vec3 pos, vec4 color\n" \
  "   :Uniforms: vec2 viewportSize, float lineWidth\n" \
  "``POLYLINE_UNIFORM_COLOR``\n" \
  "   :Attributes: vec3 pos\n" \
  "   :Uniforms: vec2 viewportSize, float lineWidth, vec4 color\n" \
  "``POINT_FLAT_COLOR``\n" \
  "   :Attributes: vec3 pos, vec4 color\n" \
  "   :Uniforms: float size\n" \
  "``POINT_UNIFORM_COLOR``\n" \
  "   :Attributes: vec3 pos\n" \
  "   :Uniforms: vec4 color, float size\n"

static const PyC_StringEnumItems pygpu_shader_builtin_items[] = {
    {GPU_SHADER_3D_FLAT_COLOR, "FLAT_COLOR"},
    {GPU_SHADER_3D_IMAGE, "IMAGE"},
    {GPU_SHADER_3D_IMAGE_SCENE_LINEAR_TO_REC709_SRGB, "IMAGE_SCENE_LINEAR_TO_REC709_SRGB"},
    {GPU_SHADER_3D_IMAGE_COLOR, "IMAGE_COLOR"},
    {GPU_SHADER_3D_IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB,
     "IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB"},
    {GPU_SHADER_3D_SMOOTH_COLOR, "SMOOTH_COLOR"},
    {GPU_SHADER_3D_UNIFORM_COLOR, "UNIFORM_COLOR"},
    {GPU_SHADER_3D_POLYLINE_FLAT_COLOR, "POLYLINE_FLAT_COLOR"},
    {GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR, "POLYLINE_SMOOTH_COLOR"},
    {GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR, "POLYLINE_UNIFORM_COLOR"},
    {GPU_SHADER_3D_POINT_FLAT_COLOR, "POINT_FLAT_COLOR"},
    {GPU_SHADER_3D_POINT_UNIFORM_COLOR, "POINT_UNIFORM_COLOR"},
    {0, nullptr},
};

static const PyC_StringEnumItems pygpu_shader_config_items[] = {
    {GPU_SHADER_CFG_DEFAULT, "DEFAULT"},
    {GPU_SHADER_CFG_CLIPPED, "CLIPPED"},
    {0, nullptr},
};

static int pygpu_shader_uniform_location_get(blender::gpu::Shader *shader,
                                             const char *name,
                                             const char *error_prefix)
{
  const int uniform = GPU_shader_get_uniform(shader, name);

  if (uniform == -1) {
    PyErr_Format(PyExc_ValueError, "%s: uniform %.32s not found", error_prefix, name);
  }

  return uniform;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Type
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_bind_doc,
    ".. method:: bind()\n"
    "\n"
    "   Bind the shader object. Required to be able to change uniforms of this shader.\n");
static PyObject *pygpu_shader_bind(BPyGPUShader *self)
{
  GPU_shader_bind(self->shader);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_from_name_doc,
    ".. method:: uniform_from_name(name)\n"
    "\n"
    "   Get uniform location by name.\n"
    "\n"
    "   :arg name: Name of the uniform variable whose location is to be queried.\n"
    "   :type name: str\n"
    "   :return: Location of the uniform variable.\n"
    "   :rtype: int\n");
static PyObject *pygpu_shader_uniform_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == nullptr) {
    return nullptr;
  }

  const int uniform = pygpu_shader_uniform_location_get(
      self->shader, name, "GPUShader.get_uniform");

  if (uniform == -1) {
    return nullptr;
  }

  return PyLong_FromLong(uniform);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_block_from_name_doc,
    ".. method:: uniform_block_from_name(name)\n"
    "\n"
    "   Get uniform block location by name.\n"
    "\n"
    "   :arg name: Name of the uniform block variable whose location is to be queried.\n"
    "   :type name: str\n"
    "   :return: The location of the uniform block variable.\n"
    "   :rtype: int\n");
static PyObject *pygpu_shader_uniform_block_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == nullptr) {
    return nullptr;
  }

  const int uniform = GPU_shader_get_uniform_block(self->shader, name);

  if (uniform == -1) {
    PyErr_Format(PyExc_ValueError, "GPUShader.get_uniform_block: uniform %.32s not found", name);
    return nullptr;
  }

  return PyLong_FromLong(uniform);
}

static bool pygpu_shader_uniform_vector_impl(PyObject *args,
                                             int elem_size,
                                             int *r_location,
                                             int *r_length,
                                             int *r_count,
                                             Py_buffer *r_pybuffer)
{
  PyObject *buffer;

  *r_count = 1;
  if (!PyArg_ParseTuple(
          args, "iOi|i:GPUShader.uniform_vector_*", r_location, &buffer, r_length, r_count))
  {
    return false;
  }

  if (PyObject_GetBuffer(buffer, r_pybuffer, PyBUF_SIMPLE) == -1) {
    /* PyObject_GetBuffer raise a PyExc_BufferError */
    return false;
  }

  if (r_pybuffer->len < (*r_length * *r_count * elem_size)) {
    PyErr_SetString(PyExc_OverflowError,
                    "GPUShader.uniform_vector_*: buffer size smaller than required.");
    return false;
  }

  return true;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_vector_float_doc,
    ".. method:: uniform_vector_float(location, buffer, length, count)\n"
    "\n"
    "   Set the buffer to fill the uniform.\n"
    "\n"
    "   :arg location: Location of the uniform variable to be modified.\n"
    "   :type location: int\n"
    "   :arg buffer: The data that should be set. Can support the buffer protocol.\n"
    "   :type buffer: Sequence[float]\n"
    "   :arg length: Size of the uniform data type:\n"
    "\n"
    "      - 1: float\n"
    "      - 2: vec2 or float[2]\n"
    "      - 3: vec3 or float[3]\n"
    "      - 4: vec4 or float[4]\n"
    "      - 9: mat3\n"
    "      - 16: mat4\n"
    "   :type length: int\n"
    "   :arg count: Specifies the number of elements, vector or matrices that are to "
    "be modified.\n"
    "   :type count: int\n");
static PyObject *pygpu_shader_uniform_vector_float(BPyGPUShader *self, PyObject *args)
{
  int location, length, count;

  Py_buffer pybuffer;

  if (!pygpu_shader_uniform_vector_impl(
          args, sizeof(float), &location, &length, &count, &pybuffer))
  {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_shader_uniform_float_ex(
      self->shader, location, length, count, static_cast<const float *>(pybuffer.buf));

  PyBuffer_Release(&pybuffer);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_vector_int_doc,
    ".. method:: uniform_vector_int(location, buffer, length, count)\n"
    "\n"
    "   See GPUShader.uniform_vector_float(...) description.\n");
static PyObject *pygpu_shader_uniform_vector_int(BPyGPUShader *self, PyObject *args)
{
  int location, length, count;

  Py_buffer pybuffer;

  if (!pygpu_shader_uniform_vector_impl(args, sizeof(int), &location, &length, &count, &pybuffer))
  {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_shader_uniform_int_ex(
      self->shader, location, length, count, static_cast<const int *>(pybuffer.buf));

  PyBuffer_Release(&pybuffer);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_bool_doc,
    ".. method:: uniform_bool(name, value)\n"
    "\n"
    "   Specify the value of a uniform variable for the current program object.\n"
    "\n"
    "   :arg name: Name of the uniform variable whose value is to be changed.\n"
    "   :type name: str\n"
    "   :arg value: Value that will be used to update the specified uniform variable.\n"
    "   :type value: bool | Sequence[bool]\n");
static PyObject *pygpu_shader_uniform_bool(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_bool";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_bool", &params.id, &params.seq)) {
    return nullptr;
  }

  int values[4];
  int length;
  int ret = -1;
  if (PySequence_Check(params.seq)) {
    PyObject *seq_fast = PySequence_Fast(params.seq, error_prefix);
    if (seq_fast == nullptr) {
      PyErr_Format(PyExc_TypeError,
                   "%s: expected a sequence, got %s",
                   error_prefix,
                   Py_TYPE(params.seq)->tp_name);
    }
    else {
      length = PySequence_Fast_GET_SIZE(seq_fast);
      if (length == 0 || length > 4) {
        PyErr_Format(PyExc_TypeError,
                     "%s: invalid sequence length. expected 1..4, got %d",
                     error_prefix,
                     length);
      }
      else {
        ret = PyC_AsArray_FAST(
            values, sizeof(*values), seq_fast, length, &PyLong_Type, error_prefix);
      }
      Py_DECREF(seq_fast);
    }
  }
  else if (((values[0] = int(PyLong_AsLong(params.seq))) != -1) && ELEM(values[0], 0, 1)) {
    length = 1;
    ret = 0;
  }
  else {
    PyErr_Format(
        PyExc_ValueError, "expected a bool or sequence, got %s", Py_TYPE(params.seq)->tp_name);
  }

  if (ret == -1) {
    return nullptr;
  }

  const int location = pygpu_shader_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_shader_uniform_int_ex(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_float_doc,
    ".. method:: uniform_float(name, value)\n"
    "\n"
    "   Specify the value of a uniform variable for the current program object.\n"
    "\n"
    "   :arg name: Name of the uniform variable whose value is to be changed.\n"
    "   :type name: str\n"
    "   :arg value: Value that will be used to update the specified uniform variable.\n"
    "   :type value: float | Sequence[float]\n");
static PyObject *pygpu_shader_uniform_float(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_float";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_float", &params.id, &params.seq)) {
    return nullptr;
  }

  float values[16];
  int length;

  if (PyFloat_Check(params.seq)) {
    values[0] = float(PyFloat_AsDouble(params.seq));
    length = 1;
  }
  else if (PyLong_Check(params.seq)) {
    values[0] = float(PyLong_AsDouble(params.seq));
    length = 1;
  }
  else if (MatrixObject_Check(params.seq)) {
    MatrixObject *mat = (MatrixObject *)params.seq;
    if (BaseMath_ReadCallback(mat) == -1) {
      return nullptr;
    }
    if ((mat->row_num != mat->col_num) || !ELEM(mat->row_num, 3, 4)) {
      PyErr_SetString(PyExc_ValueError, "Expected 3x3 or 4x4 matrix");
      return nullptr;
    }
    length = mat->row_num * mat->col_num;
    memcpy(values, mat->matrix, sizeof(float) * length);
  }
  else {
    length = mathutils_array_parse(values, 2, 16, params.seq, "");
    if (length == -1) {
      return nullptr;
    }
  }

  if (!ELEM(length, 1, 2, 3, 4, 9, 16)) {
    PyErr_SetString(PyExc_TypeError,
                    "Expected a single float or a sequence of floats of length 1..4, 9 or 16.");
    return nullptr;
  }

  const int location = pygpu_shader_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_shader_uniform_float_ex(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_int_doc,
    ".. method:: uniform_int(name, seq)\n"
    "\n"
    "   Specify the value of a uniform variable for the current program object.\n"
    "\n"
    "   :arg name: name of the uniform variable whose value is to be changed.\n"
    "   :type name: str\n"
    "   :arg seq: Value that will be used to update the specified uniform variable.\n"
    "   :type seq: Sequence[int]\n");
static PyObject *pygpu_shader_uniform_int(BPyGPUShader *self, PyObject *args)
{
  const char *error_prefix = "GPUShader.uniform_int";

  struct {
    const char *id;
    PyObject *seq;
  } params;

  if (!PyArg_ParseTuple(args, "sO:GPUShader.uniform_int", &params.id, &params.seq)) {
    return nullptr;
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
    if (seq_fast == nullptr) {
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
        ret = PyC_AsArray_FAST(
            values, sizeof(*values), seq_fast, length, &PyLong_Type, error_prefix);
      }
      Py_DECREF(seq_fast);
    }
  }
  if (ret == -1) {
    return nullptr;
  }

  const int location = pygpu_shader_uniform_location_get(self->shader, params.id, error_prefix);

  if (location == -1) {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_shader_uniform_int_ex(self->shader, location, length, 1, values);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_sampler_doc,
    ".. method:: uniform_sampler(name, texture)\n"
    "\n"
    "   Specify the value of a texture uniform variable for the current GPUShader.\n"
    "\n"
    "   :arg name: name of the uniform variable whose texture is to be specified.\n"
    "   :type name: str\n"
    "   :arg texture: Texture to attach.\n"
    "   :type texture: :class:`gpu.types.GPUTexture`\n");
static PyObject *pygpu_shader_uniform_sampler(BPyGPUShader *self, PyObject *args)
{
  const char *name;
  BPyGPUTexture *py_texture;
  if (!PyArg_ParseTuple(
          args, "sO!:GPUShader.uniform_sampler", &name, &BPyGPUTexture_Type, &py_texture))
  {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  int slot = GPU_shader_get_sampler_binding(self->shader, name);
  GPU_texture_bind(py_texture->tex, slot);
  GPU_shader_uniform_1i(self->shader, name, slot);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_image_doc,
    ".. method:: image(name, texture)\n"
    "\n"
    "   Specify the value of an image variable for the current GPUShader.\n"
    "\n"
    "   :arg name: Name of the image variable to which the texture is to be bound.\n"
    "   :type name: str\n"
    "   :arg texture: Texture to attach.\n"
    "   :type texture: :class:`gpu.types.GPUTexture`\n");
static PyObject *pygpu_shader_image(BPyGPUShader *self, PyObject *args)
{
  const char *name;
  BPyGPUTexture *py_texture;
  if (!PyArg_ParseTuple(args, "sO!:GPUShader.image", &name, &BPyGPUTexture_Type, &py_texture)) {
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  int image_unit = GPU_shader_get_sampler_binding(self->shader, name);
  if (image_unit == -1) {
    PyErr_Format(PyExc_ValueError, "Image '%s' not found in shader", name);
    return nullptr;
  }

  GPU_texture_image_bind(py_texture->tex, image_unit);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_uniform_block_doc,
    ".. method:: uniform_block(name, ubo)\n"
    "\n"
    "   Specify the value of an uniform buffer object variable for the current GPUShader.\n"
    "\n"
    "   :arg name: name of the uniform variable whose UBO is to be specified.\n"
    "   :type name: str\n"
    "   :arg ubo: Uniform Buffer to attach.\n"
    "   :type texture: :class:`gpu.types.GPUUniformBuf`\n");
static PyObject *pygpu_shader_uniform_block(BPyGPUShader *self, PyObject *args)
{
  const char *name;
  BPyGPUUniformBuf *py_ubo;
  if (!PyArg_ParseTuple(
          args, "sO!:GPUShader.uniform_block", &name, &BPyGPUUniformBuf_Type, &py_ubo))
  {
    return nullptr;
  }

  int binding = GPU_shader_get_ubo_binding(self->shader, name);
  if (binding == -1) {
    PyErr_SetString(
        PyExc_BufferError,
        "GPUShader.uniform_block: uniform block not found, make sure the name is correct");
    return nullptr;
  }

  GPU_shader_bind(self->shader);
  GPU_uniformbuf_bind(py_ubo->ubo, binding);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_attr_from_name_doc,
    ".. method:: attr_from_name(name)\n"
    "\n"
    "   Get attribute location by name.\n"
    "\n"
    "   :arg name: The name of the attribute variable whose location is to be queried.\n"
    "   :type name: str\n"
    "   :return: The location of an attribute variable.\n"
    "   :rtype: int\n");
static PyObject *pygpu_shader_attr_from_name(BPyGPUShader *self, PyObject *arg)
{
  const char *name = PyUnicode_AsUTF8(arg);
  if (name == nullptr) {
    return nullptr;
  }

  const int attr = GPU_shader_get_attribute(self->shader, name);

  if (attr == -1) {
    PyErr_Format(PyExc_ValueError, "GPUShader.attr_from_name: attribute %.32s not found", name);
    return nullptr;
  }

  return PyLong_FromLong(attr);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_format_calc_doc,
    ".. method:: format_calc()\n"
    "\n"
    "   Build a new format based on the attributes of the shader.\n"
    "\n"
    "   :return: vertex attribute format for the shader\n"
    "   :rtype: :class:`gpu.types.GPUVertFormat`\n");
static PyObject *pygpu_shader_format_calc(BPyGPUShader *self, PyObject * /*arg*/)
{
  BPyGPUVertFormat *ret = (BPyGPUVertFormat *)BPyGPUVertFormat_CreatePyObject(nullptr);
  if (bpygpu_shader_is_polyline(self->shader)) {
    GPU_vertformat_clear(&ret->fmt);

    /* WORKAROUND: Special case for POLYLINE shader. */
    if (GPU_shader_get_ssbo_binding(self->shader, "pos") >= 0) {
      GPU_vertformat_attr_add(&ret->fmt, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
    }
    if (GPU_shader_get_ssbo_binding(self->shader, "color") >= 0) {
      GPU_vertformat_attr_add(&ret->fmt, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
    }
  }
  else {
    GPU_vertformat_from_shader(&ret->fmt, self->shader);
  }
  return (PyObject *)ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_attrs_info_get_doc,
    ".. method:: attrs_info_get()\n"
    "\n"
    "   Information about the attributes used in the Shader.\n"
    "\n"
    "   :return: tuples containing information about the attributes in order (name, type)\n"
    "   :rtype: tuple[tuple[str, str | None], ...]\n");
static PyObject *pygpu_shader_attrs_info_get(BPyGPUShader *self, PyObject * /*arg*/)
{
  using namespace blender::gpu::shader;
  PyObject *ret;
  int type;
  int location_test = 0, attrs_added = 0;
  char name[256];

  if (bpygpu_shader_is_polyline(self->shader)) {
    /* WORKAROUND: Special case for POLYLINE shader. Check the SSBO inputs as attributes. */
    uint input_len = GPU_shader_get_ssbo_input_len(self->shader);

    /* Skip "gpu_index_buf". */
    input_len -= 1;
    ret = PyTuple_New(input_len);
    while (attrs_added < input_len) {
      if (!GPU_shader_get_ssbo_input_info(self->shader, location_test++, name)) {
        continue;
      }
      if (STREQ(name, "gpu_index_buf")) {
        continue;
      }

      type = STREQ(name, "pos")   ? int(Type::float3_t) :
             STREQ(name, "color") ? int(Type::float4_t) :
                                    -1;
      PyObject *py_type;
      if (type != -1) {
        py_type = PyUnicode_InternFromString(
            PyC_StringEnum_FindIDFromValue(pygpu_attrtype_items, type));
      }
      else {
        py_type = Py_None;
        Py_INCREF(py_type);
      }

      PyObject *attr_info = PyTuple_New(2);
      PyTuple_SET_ITEMS(attr_info, PyUnicode_FromString(name), py_type);
      PyTuple_SetItem(ret, attrs_added, attr_info);
      attrs_added++;
    }
  }
  else {
    uint attr_len = GPU_shader_get_attribute_len(self->shader);

    ret = PyTuple_New(attr_len);
    while (attrs_added < attr_len) {
      if (!GPU_shader_get_attribute_info(self->shader, location_test++, name, &type)) {
        continue;
      }
      PyObject *py_type;
      if (type != -1) {
        py_type = PyUnicode_InternFromString(
            PyC_StringEnum_FindIDFromValue(pygpu_attrtype_items, type));
      }
      else {
        py_type = Py_None;
        Py_INCREF(py_type);
      }

      PyObject *attr_info = PyTuple_New(2);
      PyTuple_SET_ITEMS(attr_info, PyUnicode_FromString(name), py_type);
      PyTuple_SetItem(ret, attrs_added, attr_info);
      attrs_added++;
    }
  }
  return ret;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pygpu_shader__tp_methods[] = {
    {"bind", (PyCFunction)pygpu_shader_bind, METH_NOARGS, pygpu_shader_bind_doc},
    {"uniform_from_name",
     (PyCFunction)pygpu_shader_uniform_from_name,
     METH_O,
     pygpu_shader_uniform_from_name_doc},
    {"uniform_block_from_name",
     (PyCFunction)pygpu_shader_uniform_block_from_name,
     METH_O,
     pygpu_shader_uniform_block_from_name_doc},
    {"uniform_vector_float",
     (PyCFunction)pygpu_shader_uniform_vector_float,
     METH_VARARGS,
     pygpu_shader_uniform_vector_float_doc},
    {"uniform_vector_int",
     (PyCFunction)pygpu_shader_uniform_vector_int,
     METH_VARARGS,
     pygpu_shader_uniform_vector_int_doc},
    {"uniform_bool",
     (PyCFunction)pygpu_shader_uniform_bool,
     METH_VARARGS,
     pygpu_shader_uniform_bool_doc},
    {"uniform_float",
     (PyCFunction)pygpu_shader_uniform_float,
     METH_VARARGS,
     pygpu_shader_uniform_float_doc},
    {"uniform_int",
     (PyCFunction)pygpu_shader_uniform_int,
     METH_VARARGS,
     pygpu_shader_uniform_int_doc},
    {"uniform_sampler",
     (PyCFunction)pygpu_shader_uniform_sampler,
     METH_VARARGS,
     pygpu_shader_uniform_sampler_doc},
    {"image", (PyCFunction)pygpu_shader_image, METH_VARARGS, pygpu_shader_image_doc},
    {"uniform_block",
     (PyCFunction)pygpu_shader_uniform_block,
     METH_VARARGS,
     pygpu_shader_uniform_block_doc},
    {"attr_from_name",
     (PyCFunction)pygpu_shader_attr_from_name,
     METH_O,
     pygpu_shader_attr_from_name_doc},
    {"format_calc",
     (PyCFunction)pygpu_shader_format_calc,
     METH_NOARGS,
     pygpu_shader_format_calc_doc},
    {"attrs_info_get",
     (PyCFunction)pygpu_shader_attrs_info_get,
     METH_NOARGS,
     pygpu_shader_attrs_info_get_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_name_doc,
    "The name of the shader object for debugging purposes (read-only).\n"
    "\n"
    ":type: str\n");
static PyObject *pygpu_shader_name(BPyGPUShader *self, void * /*closure*/)
{
  return PyUnicode_FromString(GPU_shader_get_name(self->shader));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_program_doc,
    "The name of the program object for use by the OpenGL API (read-only).\n"
    "This is deprecated and will always return -1.\n"
    "\n"
    ":type: int\n");
static PyObject *pygpu_shader_program_get(BPyGPUShader * /*self*/, void * /*closure*/)
{
  PyErr_WarnEx(
      PyExc_DeprecationWarning, "'program' is deprecated. No valid handle will be returned.", 1);
  return PyLong_FromLong(-1);
}

static PyGetSetDef pygpu_shader__tp_getseters[] = {
    {"program",
     (getter)pygpu_shader_program_get,
     (setter) nullptr,
     pygpu_shader_program_doc,
     nullptr},
    {"name", (getter)pygpu_shader_name, (setter) nullptr, pygpu_shader_name_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static void pygpu_shader__tp_dealloc(BPyGPUShader *self)
{
  if (self->is_builtin == false) {
    GPU_shader_free(self->shader);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

PyTypeObject BPyGPUShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUShader",
    /*tp_basicsize*/ sizeof(BPyGPUShader),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_shader__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_shader__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_shader__tp_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name gpu.shader Module API
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_unbind_doc,
    ".. function:: unbind()\n"
    "\n"
    "   Unbind the bound shader object.\n");
static PyObject *pygpu_shader_unbind(BPyGPUShader * /*self*/)
{
  GPU_shader_unbind();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_from_builtin_doc,
    ".. function:: from_builtin(shader_name, *, config='DEFAULT')\n"
    "\n"
    "   Shaders that are embedded in the blender internal code (see :ref:`built-in-shaders`).\n"
    "   They all read the uniform ``mat4 ModelViewProjectionMatrix``,\n"
    "   which can be edited by the :mod:`gpu.matrix` module.\n"
    "\n"
    "   You can also choose a shader configuration that uses clip_planes by setting the "
    "``CLIPPED`` value to the config parameter. Note that in this case you also need to "
    "manually set the value of ``mat4 ModelMatrix``.\n"
    "\n"
    "   :arg shader_name: One of the builtin shader names.\n"
    "   :type shader_name: str\n"
    "   :arg config: One of these types of shader configuration:\n"
    "\n"
    "      - ``DEFAULT``\n"
    "      - ``CLIPPED``\n"
    "   :type config: str\n"
    "   :return: Shader object corresponding to the given name.\n"
    "   :rtype: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_shader_from_builtin(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  PyC_StringEnum pygpu_bultinshader = {pygpu_shader_builtin_items};
  PyC_StringEnum pygpu_config = {pygpu_shader_config_items, GPU_SHADER_CFG_DEFAULT};

  static const char *_keywords[] = {"shader_name", "config", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `shader_name` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `config` */
      ":from_builtin",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &pygpu_bultinshader,
                                        PyC_ParseStringEnum,
                                        &pygpu_config))
  {
    return nullptr;
  }

  blender::gpu::Shader *shader = GPU_shader_get_builtin_shader_with_config(
      GPUBuiltinShader(pygpu_bultinshader.value_found), GPUShaderConfig(pygpu_config.value_found));

  if (shader == nullptr) {
    PyErr_SetString(PyExc_ValueError, "Builtin shader doesn't exist in the requested config");
    return nullptr;
  }

  return BPyGPUShader_CreatePyObject(shader, true);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_create_from_info_doc,
    ".. function:: create_from_info(shader_info)\n"
    "\n"
    "   Create shader from a GPUShaderCreateInfo.\n"
    "\n"
    "   :arg shader_info: GPUShaderCreateInfo\n"
    "   :type shader_info: :class:`gpu.types.GPUShaderCreateInfo`\n"
    "   :return: Shader object corresponding to the given name.\n"
    "   :rtype: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_shader_create_from_info(BPyGPUShader * /*self*/, BPyGPUShaderCreateInfo *o)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  if (!BPyGPUShaderCreateInfo_Check(o)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUShaderCreateInfo, got %s", Py_TYPE(o)->tp_name);
    return nullptr;
  }

  char error[128];
  if (!GPU_shader_create_info_check_error(o->info, error)) {
    PyErr_SetString(PyExc_Exception, error);
    return nullptr;
  }

  blender::gpu::Shader *shader = GPU_shader_create_from_info_python(o->info);
  if (!shader) {
    PyErr_SetString(PyExc_Exception, "Shader Compile Error, see console for more details");
    return nullptr;
  }

  return BPyGPUShader_CreatePyObject(shader, false);
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pygpu_shader_module__tp_methods[] = {
    {"unbind", (PyCFunction)pygpu_shader_unbind, METH_NOARGS, pygpu_shader_unbind_doc},
    {"from_builtin",
     (PyCFunction)pygpu_shader_from_builtin,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_shader_from_builtin_doc},
    {"create_from_info",
     (PyCFunction)pygpu_shader_create_from_info,
     METH_O,
     pygpu_shader_create_from_info_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_shader_module__tp_doc,
    "This module provides access to GPUShader internal functions.\n"
    "\n"
    ".. _built-in-shaders:\n"
    "\n"
    ".. rubric:: Built-in shaders\n"
    "\n"
    "All built-in shaders have the ``mat4 ModelViewProjectionMatrix`` uniform.\n"
    "\n"
    "Its value must be modified using the :class:`gpu.matrix` module.\n"
    "\n"
    ".. important::\n"
    "\n"
    "   Shader uniforms must be explicitly initialized to avoid retaining values from previous "
    "executions.\n"
    "\n" PYDOC_BUILTIN_SHADER_DESCRIPTION);
static PyModuleDef pygpu_shader_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.shader",
    /*m_doc*/ pygpu_shader_module__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_shader_module__tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUShader_CreatePyObject(blender::gpu::Shader *shader, bool is_builtin)
{
  BPyGPUShader *self;

  self = PyObject_New(BPyGPUShader, &BPyGPUShader_Type);
  self->shader = shader;
  self->is_builtin = is_builtin;

  return (PyObject *)self;
}

PyObject *bpygpu_shader_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_shader_module_def);

  return submodule;
}

bool bpygpu_shader_is_polyline(blender::gpu::Shader *shader)
{
  return ELEM(shader,
              GPU_shader_get_builtin_shader(GPU_SHADER_3D_POLYLINE_FLAT_COLOR),
              GPU_shader_get_builtin_shader(GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR),
              GPU_shader_get_builtin_shader(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR));
}

/** \} */
