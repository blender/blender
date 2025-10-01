/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the framebuffer functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_init_exit.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

#include "../mathutils/mathutils.hh"

#include "gpu_py.hh"
#include "gpu_py_buffer.hh"
#include "gpu_py_framebuffer.hh" /* own include */
#include "gpu_py_texture.hh"

/* -------------------------------------------------------------------- */
/** \name gpu::FrameBuffer Common Utilities
 * \{ */

static int pygpu_framebuffer_valid_check(BPyGPUFrameBuffer *bpygpu_fb)
{
  if (UNLIKELY(bpygpu_fb->fb == nullptr)) {
    PyErr_SetString(PyExc_ReferenceError, "GPU framebuffer was freed, no further access is valid");
    return -1;
  }
  return 0;
}

#define PYGPU_FRAMEBUFFER_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(pygpu_framebuffer_valid_check(bpygpu) == -1)) { \
      return nullptr; \
    } \
  } \
  ((void)0)

static void pygpu_framebuffer_free_if_possible(blender::gpu::FrameBuffer *fb)
{
  if (GPU_is_init()) {
    GPU_framebuffer_free(fb);
  }
  else {
    printf("PyFramebuffer freed after the context has been destroyed.\n");
  }
}

static void pygpu_framebuffer_free_safe(BPyGPUFrameBuffer *self)
{
  if (self->fb) {
#ifndef GPU_NO_USE_PY_REFERENCES
    GPU_framebuffer_py_reference_set(self->fb, nullptr);
    if (!self->shared_reference)
#endif
    {
      pygpu_framebuffer_free_if_possible(self->fb);
    }

    self->fb = nullptr;
  }
}

/* Keep less than or equal to #FRAMEBUFFER_STACK_DEPTH */
#define GPU_PY_FRAMEBUFFER_STACK_LEN 16

static bool pygpu_framebuffer_stack_push_and_bind_or_error(blender::gpu::FrameBuffer *fb)
{
  if (GPU_framebuffer_stack_level_get() >= GPU_PY_FRAMEBUFFER_STACK_LEN) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "Maximum framebuffer stack depth " STRINGIFY(GPU_PY_FRAMEBUFFER_STACK_LEN) " reached");
    return false;
  }
  GPU_framebuffer_push(GPU_framebuffer_active_get());
  GPU_framebuffer_bind(fb);
  return true;
}

static bool pygpu_framebuffer_stack_pop_and_restore_or_error(blender::gpu::FrameBuffer *fb)
{
  if (GPU_framebuffer_stack_level_get() == 0) {
    PyErr_SetString(PyExc_RuntimeError, "Minimum framebuffer stack depth reached");
    return false;
  }

  if (fb && !GPU_framebuffer_bound(fb)) {
    PyErr_SetString(PyExc_RuntimeError, "Framebuffer is not bound");
    return false;
  }

  blender::gpu::FrameBuffer *fb_prev = GPU_framebuffer_pop();
  GPU_framebuffer_bind(fb_prev);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stack (Context Manager)
 *
 * Safer alternative to ensure balanced push/pop calls.
 *
 * \{ */

struct PyFrameBufferStackContext {
  PyObject_HEAD /* Required Python macro. */
  BPyGPUFrameBuffer *py_fb;
  int level;
};

static void pygpu_framebuffer_stack_context__tp_dealloc(PyFrameBufferStackContext *self)
{
  Py_DECREF(self->py_fb);
  PyObject_DEL(self);
}

static PyObject *pygpu_framebuffer_stack_context_enter(PyFrameBufferStackContext *self)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self->py_fb);

  /* sanity - should never happen */
  if (self->level != -1) {
    PyErr_SetString(PyExc_RuntimeError, "Already in use");
    return nullptr;
  }

  if (!pygpu_framebuffer_stack_push_and_bind_or_error(self->py_fb->fb)) {
    return nullptr;
  }

  self->level = GPU_framebuffer_stack_level_get();
  Py_RETURN_NONE;
}

static PyObject *pygpu_framebuffer_stack_context_exit(PyFrameBufferStackContext *self,
                                                      PyObject * /*args*/)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self->py_fb);

  /* sanity - should never happen */
  if (self->level == -1) {
    fprintf(stderr, "Not yet in use\n");
    return nullptr;
  }

  const int level = GPU_framebuffer_stack_level_get();
  if (level != self->level) {
    fprintf(stderr, "Level of bind mismatch, expected %d, got %d\n", self->level, level);
  }

  if (!pygpu_framebuffer_stack_pop_and_restore_or_error(self->py_fb->fb)) {
    return nullptr;
  }
  Py_RETURN_NONE;
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

static PyMethodDef pygpu_framebuffer_stack_context__tp_methods[] = {
    {"__enter__", (PyCFunction)pygpu_framebuffer_stack_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)pygpu_framebuffer_stack_context_exit, METH_VARARGS},
    {nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyTypeObject FramebufferStackContext_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUFrameBufferStackContext",
    /*tp_basicsize*/ sizeof(PyFrameBufferStackContext),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_framebuffer_stack_context__tp_dealloc,
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
    /*tp_methods*/ pygpu_framebuffer_stack_context__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
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

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_bind_doc,
    ".. function:: bind()\n"
    "\n"
    "   Context manager to ensure balanced bind calls, even in the case of an error.\n");
static PyObject *pygpu_framebuffer_bind(BPyGPUFrameBuffer *self)
{
  PyFrameBufferStackContext *ret = PyObject_New(PyFrameBufferStackContext,
                                                &FramebufferStackContext_Type);
  ret->py_fb = self;
  ret->level = -1;
  Py_INCREF(self);
  return (PyObject *)ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUFramebuffer Type
 * \{ */

/* Fill in the GPUAttachment according to the PyObject parameter.
 * PyObject *o can be nullptr, Py_None, BPyGPUTexture or a dictionary containing the keyword
 * "texture" and the optional keywords "layer" and "mip". Returns false on error. In this case, a
 * python message will be raised and GPUAttachment will not be touched. */
static bool pygpu_framebuffer_new_parse_arg(PyObject *o, GPUAttachment *r_attach)
{
  GPUAttachment tmp_attach = GPU_ATTACHMENT_NONE;

  if (!o || o == Py_None) {
    /* Pass. */;
  }
  else if (BPyGPUTexture_Check(o)) {
    if (!bpygpu_ParseTexture(o, &tmp_attach.tex)) {
      return false;
    }
  }
  else {
    const char *c_texture = "texture";
    const char *c_layer = "layer";
    const char *c_mip = "mip";
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(o, &pos, &key, &value)) {
      if (!PyUnicode_Check(key)) {
        PyErr_SetString(PyExc_TypeError, "keywords must be strings");
        return false;
      }

      if (c_texture && PyUnicode_CompareWithASCIIString(key, c_texture) == 0) {
        /* Compare only once. */
        c_texture = nullptr;
        if (!bpygpu_ParseTexture(value, &tmp_attach.tex)) {
          return false;
        }
      }
      else if (c_layer && PyUnicode_CompareWithASCIIString(key, c_layer) == 0) {
        /* Compare only once. */
        c_layer = nullptr;
        tmp_attach.layer = PyLong_AsLong(value);
        if (tmp_attach.layer == -1 && PyErr_Occurred()) {
          return false;
        }
      }
      else if (c_mip && PyUnicode_CompareWithASCIIString(key, c_mip) == 0) {
        /* Compare only once. */
        c_mip = nullptr;
        tmp_attach.mip = PyLong_AsLong(value);
        if (tmp_attach.mip == -1 && PyErr_Occurred()) {
          return false;
        }
      }
      else {
        PyErr_Format(
            PyExc_TypeError, "'%U' is an invalid keyword argument for this attribute", key);
        return false;
      }
    }
  }

  *r_attach = tmp_attach;
  return true;
}

static PyObject *pygpu_framebuffer__tp_new(PyTypeObject * /*self*/, PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  if (!GPU_context_active_get()) {
    PyErr_SetString(PyExc_RuntimeError, "No active GPU context found");
    return nullptr;
  }

  PyObject *depth_attachment = nullptr;
  PyObject *color_attachements = nullptr;
  static const char *_keywords[] = {"depth_slot", "color_slots", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O"  /* `depth_slot` */
      "O"  /* `color_slots` */
      ":GPUFrameBuffer.__new__",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &depth_attachment, &color_attachements))
  {
    return nullptr;
  }

/* Keep in sync with #GPU_FB_MAX_COLOR_ATTACHMENT.
 * TODO: share the define. */
#define BPYGPU_FB_MAX_COLOR_ATTACHMENT 6

  GPUAttachment config[BPYGPU_FB_MAX_COLOR_ATTACHMENT + 1];

  if (!pygpu_framebuffer_new_parse_arg(depth_attachment, &config[0])) {
    return nullptr;
  }
  if (config[0].tex && !GPU_texture_has_depth_format(config[0].tex)) {
    PyErr_SetString(PyExc_ValueError, "Depth texture with incompatible format");
    return nullptr;
  }

  int color_attachements_len = 0;
  if (color_attachements && color_attachements != Py_None) {
    if (PySequence_Check(color_attachements)) {
      color_attachements_len = PySequence_Size(color_attachements);
      if (color_attachements_len > BPYGPU_FB_MAX_COLOR_ATTACHMENT) {
        PyErr_SetString(PyExc_AttributeError,
                        "too many attachments, max is " STRINGIFY(BPYGPU_FB_MAX_COLOR_ATTACHMENT));
        return nullptr;
      }

      for (int i = 0; i < color_attachements_len; i++) {
        PyObject *o = PySequence_GetItem(color_attachements, i);
        bool ok = pygpu_framebuffer_new_parse_arg(o, &config[i + 1]);
        Py_DECREF(o);
        if (!ok) {
          return nullptr;
        }
      }
    }
    else {
      if (!pygpu_framebuffer_new_parse_arg(color_attachements, &config[1])) {
        return nullptr;
      }
      color_attachements_len = 1;
    }
  }

  blender::gpu::FrameBuffer *fb_python = GPU_framebuffer_create("fb_python");
  GPU_framebuffer_config_array(fb_python, config, color_attachements_len + 1);

  return BPyGPUFrameBuffer_CreatePyObject(fb_python, false);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_is_bound_doc,
    "Checks if this is the active frame-buffer in the context.");
static PyObject *pygpu_framebuffer_is_bound(BPyGPUFrameBuffer *self, void * /*type*/)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);
  return PyBool_FromLong(GPU_framebuffer_bound(self->fb));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_clear_doc,
    ".. method:: clear(*, color=None, depth=None, stencil=None)\n"
    "\n"
    "   Fill color, depth and stencil textures with specific value.\n"
    "   Common values: color=(0.0, 0.0, 0.0, 1.0), depth=1.0, stencil=0.\n"
    "\n"
    "   :arg color: Sequence of 3 or 4 floats representing ``(r, g, b, a)``.\n"
    "   :type color: Sequence[float]\n"
    "   :arg depth: depth value.\n"
    "   :type depth: float\n"
    "   :arg stencil: stencil value.\n"
    "   :type stencil: int\n");
static PyObject *pygpu_framebuffer_clear(BPyGPUFrameBuffer *self, PyObject *args, PyObject *kwds)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);

  if (!GPU_framebuffer_bound(self->fb)) {
    return nullptr;
  }

  PyObject *py_col = nullptr;
  PyObject *py_depth = nullptr;
  PyObject *py_stencil = nullptr;

  static const char *_keywords[] = {"color", "depth", "stencil", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O"  /* `color` */
      "O"  /* `depth` */
      "O"  /* `stencil` */
      ":clear",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &py_col, &py_depth, &py_stencil)) {
    return nullptr;
  }

  GPUFrameBufferBits buffers = GPUFrameBufferBits(0);
  float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float depth = 1.0f;
  uint stencil = 0;

  if (py_col && py_col != Py_None) {
    if (mathutils_array_parse(
            col, 3, 4, py_col, "gpu::FrameBuffer.clear(), invalid 'color' arg") == -1)
    {
      return nullptr;
    }
    buffers |= GPU_COLOR_BIT;
  }

  if (py_depth && py_depth != Py_None) {
    depth = PyFloat_AsDouble(py_depth);
    if (PyErr_Occurred()) {
      return nullptr;
    }
    buffers |= GPU_DEPTH_BIT;
  }

  if (py_stencil && py_stencil != Py_None) {
    if ((stencil = PyC_Long_AsU32(py_stencil)) == uint(-1)) {
      return nullptr;
    }
    buffers |= GPU_STENCIL_BIT;
  }

  GPU_framebuffer_clear(self->fb, buffers, col, depth, stencil);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_viewport_set_doc,
    ".. function:: viewport_set(x, y, xsize, ysize)\n"
    "\n"
    "   Set the viewport for this framebuffer object.\n"
    "   Note: The viewport state is not saved upon framebuffer rebind.\n"
    "\n"
    "   :arg x, y: lower left corner of the viewport_set rectangle, in pixels.\n"
    "   :type x, y: int\n"
    "   :arg xsize, ysize: width and height of the viewport_set.\n"
    "   :type xsize, ysize: int\n");
static PyObject *pygpu_framebuffer_viewport_set(BPyGPUFrameBuffer *self,
                                                PyObject *args,
                                                void * /*type*/)
{
  int x, y, xsize, ysize;
  if (!PyArg_ParseTuple(args, "iiii:viewport_set", &x, &y, &xsize, &ysize)) {
    return nullptr;
  }

  GPU_framebuffer_viewport_set(self->fb, x, y, xsize, ysize);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_viewport_get_doc,
    ".. function:: viewport_get()\n"
    "\n"
    "   Returns position and dimension to current viewport.\n");
static PyObject *pygpu_framebuffer_viewport_get(BPyGPUFrameBuffer *self, void * /*type*/)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);
  int viewport[4];
  GPU_framebuffer_viewport_get(self->fb, viewport);

  PyObject *ret = PyTuple_New(4);
  PyTuple_SET_ITEMS(ret,
                    PyLong_FromLong(viewport[0]),
                    PyLong_FromLong(viewport[1]),
                    PyLong_FromLong(viewport[2]),
                    PyLong_FromLong(viewport[3]));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_read_color_doc,
    ".. function:: read_color(x, y, xsize, ysize, channels, slot, format, *, data=data)\n"
    "\n"
    "   Read a block of pixels from the frame buffer.\n"
    "\n"
    "   :arg x, y: Lower left corner of a rectangular block of pixels.\n"
    "   :arg xsize, ysize: Dimensions of the pixel rectangle.\n"
    "   :type x, y, xsize, ysize: int\n"
    "   :arg channels: Number of components to read.\n"
    "   :type channels: int\n"
    "   :arg slot: The framebuffer slot to read data from.\n"
    "   :type slot: int\n"
    "   :arg format: The format that describes the content of a single channel.\n"
    "      Possible values are ``FLOAT``, ``INT``, ``UINT``, ``UBYTE``, ``UINT_24_8`` & "
    "``10_11_11_REV``.\n"
    "      ``UINT_24_8`` is deprecated, use ``FLOAT`` instead.\n"
    "   :type format: str\n"
    "   :arg data: Optional Buffer object to fill with the pixels values.\n"
    "   :type data: :class:`gpu.types.Buffer`\n"
    "   :return: The Buffer with the read pixels.\n"
    "   :rtype: :class:`gpu.types.Buffer`\n");
static PyObject *pygpu_framebuffer_read_color(BPyGPUFrameBuffer *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);
  int x, y, w, h, channels;
  uint slot;
  PyC_StringEnum pygpu_dataformat = {bpygpu_dataformat_items,
                                     int(blender::gpu::TextureFormat::UNORM_8_8_8_8)};
  BPyGPUBuffer *py_buffer = nullptr;

  static const char *_keywords[] = {
      "x", "y", "xsize", "ysize", "channels", "slot", "format", "data", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "i"  /* `x` */
      "i"  /* `y` */
      "i"  /* `xsize` */
      "i"  /* `ysize` */
      "i"  /* `channels` */
      "I"  /* `slot` */
      "O&" /* `format` */
      "|$" /* Optional keyword only arguments. */
      "O!" /* `data` */
      ":read_color",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &x,
                                        &y,
                                        &w,
                                        &h,
                                        &channels,
                                        &slot,
                                        PyC_ParseStringEnum,
                                        &pygpu_dataformat,
                                        &BPyGPU_BufferType,
                                        &py_buffer))
  {
    return nullptr;
  }

  if (pygpu_dataformat.value_found == GPU_DATA_UINT_24_8_DEPRECATED) {
    PyErr_WarnEx(PyExc_DeprecationWarning, "`UINT_24_8` is deprecated, use `FLOAT` instead", 1);
  }

  if (!IN_RANGE_INCL(channels, 1, 4)) {
    PyErr_SetString(PyExc_AttributeError, "Color channels must be 1, 2, 3 or 4");
    return nullptr;
  }

  if (slot >= BPYGPU_FB_MAX_COLOR_ATTACHMENT) {
    PyErr_SetString(PyExc_ValueError, "slot overflow");
    return nullptr;
  }

  if (py_buffer) {
    if (pygpu_dataformat.value_found != py_buffer->format) {
      PyErr_SetString(PyExc_AttributeError,
                      "the format of the buffer is different from that specified");
      return nullptr;
    }

    size_t size_curr = bpygpu_Buffer_size(py_buffer);
    size_t size_expected = w * h * channels *
                           GPU_texture_dataformat_size(
                               eGPUDataFormat(pygpu_dataformat.value_found));
    if (size_curr < size_expected) {
      PyErr_SetString(PyExc_BufferError, "the buffer size is smaller than expected");
      return nullptr;
    }
    Py_INCREF(py_buffer);
  }
  else {
    const Py_ssize_t shape[3] = {h, w, channels};
    py_buffer = BPyGPU_Buffer_CreatePyObject(pygpu_dataformat.value_found, shape, 3, nullptr);
    BLI_assert(bpygpu_Buffer_size(py_buffer) ==
               size_t(w) * size_t(h) * size_t(channels) *
                   GPU_texture_dataformat_size(eGPUDataFormat(pygpu_dataformat.value_found)));
  }

  GPU_framebuffer_read_color(self->fb,
                             x,
                             y,
                             w,
                             h,
                             channels,
                             int(slot),
                             eGPUDataFormat(pygpu_dataformat.value_found),
                             py_buffer->buf.as_void);

  return (PyObject *)py_buffer;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_read_depth_doc,
    ".. function:: read_depth(x, y, xsize, ysize, *, data=data)\n"
    "\n"
    "   Read a pixel depth block from the frame buffer.\n"
    "\n"
    "   :arg x, y: Lower left corner of a rectangular block of pixels.\n"
    "   :type x, y: int\n"
    "   :arg xsize, ysize: Dimensions of the pixel rectangle.\n"
    "   :type xsize, ysize: int\n"
    "   :arg data: Optional Buffer object to fill with the pixels values.\n"
    "   :type data: :class:`gpu.types.Buffer`\n"
    "   :return: The Buffer with the read pixels.\n"
    "   :rtype: :class:`gpu.types.Buffer`\n");
static PyObject *pygpu_framebuffer_read_depth(BPyGPUFrameBuffer *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);
  int x, y, w, h;
  BPyGPUBuffer *py_buffer = nullptr;

  static const char *_keywords[] = {"x", "y", "xsize", "ysize", "data", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "i"  /* `x` */
      "i"  /* `y` */
      "i"  /* `xsize` */
      "i"  /* `ysize` */
      "|$" /* Optional keyword only arguments. */
      "O!" /* `data` */
      ":read_depth",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &x, &y, &w, &h, &BPyGPU_BufferType, &py_buffer))
  {
    return nullptr;
  }

  if (py_buffer) {
    if (py_buffer->format != GPU_DATA_FLOAT) {
      PyErr_SetString(PyExc_AttributeError, "the format of the buffer must be 'GPU_DATA_FLOAT'");
      return nullptr;
    }

    size_t size_curr = bpygpu_Buffer_size(py_buffer);
    size_t size_expected = w * h * GPU_texture_dataformat_size(GPU_DATA_FLOAT);
    if (size_curr < size_expected) {
      PyErr_SetString(PyExc_BufferError, "the buffer size is smaller than expected");
      return nullptr;
    }
    Py_INCREF(py_buffer);
  }
  else {
    const Py_ssize_t shape[2] = {h, w};
    py_buffer = BPyGPU_Buffer_CreatePyObject(GPU_DATA_FLOAT, shape, 2, nullptr);
    BLI_assert(bpygpu_Buffer_size(py_buffer) ==
               w * h * GPU_texture_dataformat_size(GPU_DATA_FLOAT));
  }

  GPU_framebuffer_read_depth(self->fb, x, y, w, h, GPU_DATA_FLOAT, py_buffer->buf.as_void);

  return (PyObject *)py_buffer;
}

#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer_free_doc,
    ".. method:: free()\n"
    "\n"
    "   Free the framebuffer object.\n"
    "   The framebuffer will no longer be accessible.\n");
static PyObject *pygpu_framebuffer_free(BPyGPUFrameBuffer *self)
{
  PYGPU_FRAMEBUFFER_CHECK_OBJ(self);
  pygpu_framebuffer_free_safe(self);
  Py_RETURN_NONE;
}
#endif

static void BPyGPUFrameBuffer__tp_dealloc(BPyGPUFrameBuffer *self)
{
  pygpu_framebuffer_free_safe(self);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef pygpu_framebuffer__tp_getseters[] = {
    {"is_bound",
     (getter)pygpu_framebuffer_is_bound,
     (setter) nullptr,
     pygpu_framebuffer_is_bound_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pygpu_framebuffer__tp_methods[] = {
    {"bind", (PyCFunction)pygpu_framebuffer_bind, METH_NOARGS, pygpu_framebuffer_bind_doc},
    {"clear",
     (PyCFunction)pygpu_framebuffer_clear,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_framebuffer_clear_doc},
    {"viewport_set",
     (PyCFunction)pygpu_framebuffer_viewport_set,
     METH_NOARGS,
     pygpu_framebuffer_viewport_set_doc},
    {"viewport_get",
     (PyCFunction)pygpu_framebuffer_viewport_get,
     METH_NOARGS,
     pygpu_framebuffer_viewport_get_doc},
    {"read_color",
     (PyCFunction)pygpu_framebuffer_read_color,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_framebuffer_read_color_doc},
    {"read_depth",
     (PyCFunction)pygpu_framebuffer_read_depth,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_framebuffer_read_depth_doc},
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
    {"free", (PyCFunction)pygpu_framebuffer_free, METH_NOARGS, pygpu_framebuffer_free_doc},
#endif
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/* Ideally type aliases would de-duplicate:
 * `GPUTexture | dict[str, int | GPUTexture]` in this doc-string. */
PyDoc_STRVAR(
    /* Wrap. */
    pygpu_framebuffer__tp_doc,
    ".. class:: GPUFrameBuffer(*, depth_slot=None, color_slots=None)\n"
    "\n"
    "   This object gives access to framebuffer functionalities.\n"
    "   When a 'layer' is specified in a argument, a single layer of a 3D or array "
    "texture is attached to the frame-buffer.\n"
    "   For cube map textures, layer is translated into a cube map face.\n"
    "\n"
    "   :arg depth_slot: GPUTexture to attach or a ``dict`` containing keywords: "
    "'texture', 'layer' and 'mip'.\n"
    "   :type depth_slot: :class:`gpu.types.GPUTexture` | dict[] | None\n"
    "   :arg color_slots: Tuple where each item can be a GPUTexture or a ``dict`` "
    "containing keywords: 'texture', 'layer' and 'mip'.\n"
    "   :type color_slots: :class:`gpu.types.GPUTexture` | "
    "dict[str, int | :class:`gpu.types.GPUTexture`] | "
    "Sequence[:class:`gpu.types.GPUTexture` | dict[str, int | "
    ":class:`gpu.types.GPUTexture`]] | "
    "None\n");
PyTypeObject BPyGPUFrameBuffer_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUFrameBuffer",
    /*tp_basicsize*/ sizeof(BPyGPUFrameBuffer),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BPyGPUFrameBuffer__tp_dealloc,
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
    /*tp_doc*/ pygpu_framebuffer__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_framebuffer__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_framebuffer__tp_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_framebuffer__tp_new,
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
/** \name Public API
 * \{ */

PyObject *BPyGPUFrameBuffer_CreatePyObject(blender::gpu::FrameBuffer *fb, bool shared_reference)
{
  BPyGPUFrameBuffer *self;

#ifndef GPU_NO_USE_PY_REFERENCES
  if (shared_reference) {
    void **ref = GPU_framebuffer_py_reference_get(fb);
    if (ref) {
      /* Retrieve BPyGPUFrameBuffer reference. */
      self = (BPyGPUFrameBuffer *)POINTER_OFFSET(ref, -offsetof(BPyGPUFrameBuffer, fb));
      BLI_assert(self->fb == fb);
      Py_INCREF(self);
      return (PyObject *)self;
    }
  }
#else
  UNUSED_VARS(shared_reference);
#endif

  self = PyObject_New(BPyGPUFrameBuffer, &BPyGPUFrameBuffer_Type);
  self->fb = fb;

#ifndef GPU_NO_USE_PY_REFERENCES
  self->shared_reference = shared_reference;

  BLI_assert(GPU_framebuffer_py_reference_get(fb) == nullptr);
  GPU_framebuffer_py_reference_set(fb, (void **)&self->fb);
#endif

  return (PyObject *)self;
}

/** \} */

#undef PYGPU_FRAMEBUFFER_CHECK_OBJ
