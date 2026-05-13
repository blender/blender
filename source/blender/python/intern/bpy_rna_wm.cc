/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file extends the window-manager with C/Python API methods and attributes.
 */

#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh"

#include "WM_api.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_capi_utils.hh"
#include "bpy_rna.hh"
#include "bpy_rna_callback.hh"
#include "bpy_rna_wm.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Window Manager Clipboard Property
 *
 * Avoid using the RNA API because this value may change between checking its length
 * and creating the buffer, causing writes past the allocated length.
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_WindowManager_clipboard_doc,
    "Clipboard text storage.\n"
    "\n"
    ":type: str\n");
static PyObject *pyrna_WindowManager_clipboard_get(PyObject * /*self*/, void * /*flag*/)
{
  int text_len = 0;
  /* No need for UTF8 validation as #PyC_UnicodeFromBytesAndSize handles invalid byte sequences. */
  char *text = WM_clipboard_text_get(false, false, &text_len);
  PyObject *result = PyC_UnicodeFromBytesAndSize(text ? text : "", text_len);
  if (text != nullptr) {
    MEM_delete(text);
  }
  return result;
}

static int pyrna_WindowManager_clipboard_set(PyObject * /*self*/, PyObject *value, void * /*flag*/)
{
  PyObject *value_coerce = nullptr;
  const char *text = PyC_UnicodeAsBytes(value, &value_coerce);
  if (text == nullptr) {
    return -1;
  }
  WM_clipboard_text_set(text, false);
  Py_XDECREF(value_coerce);
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Manager Type
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_draw_cursor_add_doc,
    ".. classmethod:: draw_cursor_add(callback, args, space_type, region_type)\n"
    "\n"
    "   Add a new draw cursor handler to this space type.\n"
    "   It will be called every time the cursor for the specified region in the space "
    "type will be drawn.\n"
    "   Note: All arguments are positional only for now.\n"
    "\n"
    "   :param callback:\n"
    "      A function that will be called when the cursor is drawn.\n"
    "      It gets the specified arguments as input with the mouse position "
    "(``tuple[int, int]``) as last argument.\n"
    "   :type callback: Callable[..., Any]\n"
    "   :param args: Arguments that will be passed to the callback.\n"
    "   :type args: tuple[Any, ...]\n"
    "   :param space_type: The space type the callback draws in; for example ``VIEW_3D``. "
    "(:class:`bpy.types.Space.type`)\n"
    "   :type space_type: str\n"
    "   :param region_type: The region type the callback draws in; usually ``WINDOW``. "
    "(:class:`bpy.types.Region.type`)\n"
    "   :type region_type: str\n"
    "   :return: Handler that can be removed later on.\n"
    "   :rtype: object\n");
PyDoc_STRVAR(
    /* Wrap. */
    pyrna_draw_cursor_remove_doc,
    ".. classmethod:: draw_cursor_remove(handler)\n"
    "\n"
    "   Remove a draw cursor handler that was added previously.\n"
    "\n"
    "   :param handler: The draw cursor handler that should be removed.\n"
    "   :type handler: object\n");

PyMethodDef BPY_rna_windowmanager_draw_cursor_add_method_def = {
    "draw_cursor_add",
    static_cast<PyCFunction>(pyrna_callback_classmethod_add),
    METH_VARARGS | METH_CLASS,
    pyrna_draw_cursor_add_doc,
};

PyMethodDef BPY_rna_windowmanager_draw_cursor_remove_method_def = {
    "draw_cursor_remove",
    static_cast<PyCFunction>(pyrna_callback_classmethod_remove),
    METH_VARARGS | METH_CLASS,
    pyrna_draw_cursor_remove_doc,
};

PyGetSetDef BPY_rna_windowmanager_clipboard_getset_def = {
    "clipboard",
    pyrna_WindowManager_clipboard_get,
    pyrna_WindowManager_clipboard_set,
    pyrna_WindowManager_clipboard_doc,
    nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Screenshot
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_rna_window_screenshot_doc,
    ".. method:: screenshot(*, region=None, use_alpha=False)\n"
    "\n"
    "   Capture the windows pixel data.\n"
    "\n"
    "   :param region: The region to capture, or ``None`` to capture all.\n"
    "      Each int pair represents a pixel coordinate "
    "(the end value is not inclusive, matching Python slicing): "
    "((min_x, min_y), (max_x, max_y))\n"
    "   :type region: tuple[tuple[int, int], tuple[int, int]] | None\n"
    "   :param use_alpha: When false the alpha channel is fully opaque. "
    "Otherwise alpha values from the window's frame-buffer are returned as-is.\n"
    "   :type use_alpha: bool\n"
    "   :return: A read-only :class:`memoryview` of shape ``(height, width, 4)`` "
    "and format ``'B'``, viewing the captured RGBA pixels (rows ordered from bottom to top).\n"
    "   :rtype: memoryview\n");
static PyObject *bpy_rna_window_screenshot(PyObject *self, PyObject *args, PyObject *kwds)
{
  BPy_StructRNA *pyrna = reinterpret_cast<BPy_StructRNA *>(self);
  wmWindow *win = static_cast<wmWindow *>(pyrna->ptr->data);

  std::optional<rcti> region;
  bool use_alpha = false;

  static const char *_keywords[] = {"region", "use_alpha", nullptr};
  static _PyArg_Parser _parser = {
      "|$" /* Optional keyword only arguments. */
      "O&" /* `region` */
      "O&" /* `use_alpha` */
      ":screenshot",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, PyC_ParseOptionalRectI, &region, PyC_ParseBool, &use_alpha))
  {
    return nullptr;
  }

  if (G.background) {
    PyErr_SetString(PyExc_RuntimeError, "Window.screenshot() is not available in background mode");
    return nullptr;
  }

  bContext *C = BPY_context_get();

  int dumprect_size[2];
  uint8_t *dumprect = WM_window_pixels_read(C, win, dumprect_size);
  if (dumprect == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Failed to read window pixels");
    return nullptr;
  }

  if (region) {
    /* `xmax` / `ymax` are end-exclusive (Python slicing); clamp to `[0, dim]`,
     * keeping `max >= min` so an inverted/out-of-range region collapses to a
     * zero-sized rect rather than a negative one. */
    rcti safe = {
        .xmin = max_ii(0, region->xmin),
        .xmax = min_ii(dumprect_size[0], region->xmax),
        .ymin = max_ii(0, region->ymin),
        .ymax = min_ii(dumprect_size[1], region->ymax),
    };
    safe.xmax = max_ii(safe.xmin, safe.xmax);
    safe.ymax = max_ii(safe.ymin, safe.ymax);

    const int2 crop_size = {BLI_rcti_size_x(&safe), BLI_rcti_size_y(&safe)};
    if (crop_size.x > 0 && crop_size.y > 0) {
      const size_t row_bytes = size_t(crop_size.x) * 4;
      const size_t src_stride = size_t(dumprect_size[0]) * 4;

      uint8_t *cropped = MEM_new_array_uninitialized<uint8_t>(row_bytes * crop_size.y, __func__);
      for (int y = 0; y < crop_size.y; y++) {
        const uint8_t *src = dumprect + (size_t(safe.ymin + y) * src_stride) +
                             (size_t(safe.xmin) * 4);
        memcpy(cropped + size_t(y) * row_bytes, src, row_bytes);
      }
      MEM_delete(dumprect);
      dumprect = cropped;
      dumprect_size[0] = crop_size.x;
      dumprect_size[1] = crop_size.y;
    }
    else {
      /* Empty region: free the full-window buffer and substitute a 1-byte
       * placeholder so #PyC_MemoryView_FromBufferOwned still receives a
       * non-null `info->buf`; the resulting memoryview has shape (0, 0, 4)
       * and zero length, so the placeholder is never accessed. */
      MEM_delete(dumprect);
      dumprect = MEM_new_array_uninitialized<uint8_t>(1, __func__);
      dumprect_size[0] = 0;
      dumprect_size[1] = 0;
    }
  }

  /* Force alpha to fully opaque unless the caller asked for raw alpha values. */
  if (!use_alpha) {
    const size_t total_bytes = size_t(dumprect_size[0]) * dumprect_size[1] * 4;
    for (size_t i = 3; i < total_bytes; i += 4) {
      dumprect[i] = 0xff;
    }
  }

  Py_ssize_t shape[3] = {dumprect_size[1], dumprect_size[0], 4};
  Py_ssize_t strides[3] = {Py_ssize_t(dumprect_size[0]) * 4, 4, 1};
  Py_buffer info;
  memset(&info, 0, sizeof(info));
  info.buf = dumprect;
  info.len = shape[0] * shape[1] * shape[2];
  info.itemsize = 1;
  info.readonly = 1;
  info.format = const_cast<char *>("B");
  info.ndim = 3;
  info.shape = shape;
  info.strides = strides;
  return PyC_MemoryView_FromBufferOwned(&info);
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

PyMethodDef BPY_rna_window_screenshot_method_def = {
    "screenshot",
    reinterpret_cast<PyCFunction>(bpy_rna_window_screenshot),
    METH_VARARGS | METH_KEYWORDS,
    bpy_rna_window_screenshot_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

}  // namespace blender
