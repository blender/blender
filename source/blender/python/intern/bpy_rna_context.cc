/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file adds some helper methods to the context, that cannot fit well in RNA itself.
 */

#include <Python.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bpy_rna_context.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "bpy_rna.h"

/* -------------------------------------------------------------------- */
/** \name Temporary Context Override (Python Context Manager)
 * \{ */

struct ContextStore {
  wmWindow *win;
  bool win_is_set;
  ScrArea *area;
  bool area_is_set;
  ARegion *region;
  bool region_is_set;
};

struct BPyContextTempOverride {
  PyObject_HEAD /* Required Python macro. */
  bContext *context;

  ContextStore ctx_init;
  ContextStore ctx_temp;
  /** Bypass Python overrides set when calling an operator from Python. */
  bContext_PyState py_state;
  /**
   * This dictionary is used to store members that don't have special handling,
   * see: #bpy_context_temp_override_extract_known_args,
   * these will then be accessed via #BPY_context_member_get.
   *
   * This also supports nested *stacking*, so a nested temp-context-overrides
   * will overlay the new members on the old members (instead of ignoring them).
   */
  PyObject *py_state_context_dict;
};

static void bpy_rna_context_temp_override__tp_dealloc(BPyContextTempOverride *self)
{
  PyObject_DEL(self);
}

static PyObject *bpy_rna_context_temp_override_enter(BPyContextTempOverride *self)
{
  bContext *C = self->context;

  CTX_py_state_push(C, &self->py_state, self->py_state_context_dict);

  self->ctx_init.win = CTX_wm_window(C);
  self->ctx_init.area = CTX_wm_area(C);
  self->ctx_init.region = CTX_wm_region(C);

  wmWindow *win = self->ctx_temp.win_is_set ? self->ctx_temp.win : self->ctx_init.win;
  ScrArea *area = self->ctx_temp.area_is_set ? self->ctx_temp.area : self->ctx_init.area;
  ARegion *region = self->ctx_temp.region_is_set ? self->ctx_temp.region : self->ctx_init.region;

  self->ctx_init.win_is_set = (self->ctx_init.win != win);
  self->ctx_init.area_is_set = (self->ctx_init.area != area);
  self->ctx_init.region_is_set = (self->ctx_init.region != region);

  bScreen *screen = win ? WM_window_get_active_screen(win) : nullptr;

  /* Sanity check, the region is in the screen/area. */
  if (self->ctx_temp.region_is_set && (region != nullptr)) {
    if (area == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Region set with nullptr area");
      return nullptr;
    }
    if ((screen && BLI_findindex(&screen->regionbase, region) == -1) &&
        (BLI_findindex(&area->regionbase, region) == -1))
    {
      PyErr_SetString(PyExc_TypeError, "Region not found in area");
      return nullptr;
    }
  }

  if (self->ctx_temp.area_is_set && (area != nullptr)) {
    if (screen == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Area set with nullptr screen");
      return nullptr;
    }
    if (BLI_findindex(&screen->areabase, area) == -1) {
      PyErr_SetString(PyExc_TypeError, "Area not found in screen");
      return nullptr;
    }
  }

  /* NOTE: always set these members, even when they are equal to the current values because
   * setting the window (for e.g.) clears the area & region, setting the area clears the region.
   * While it would be useful in some cases to leave the context as-is when setting members
   * to their current values.
   *
   * Favor predictable behavior, where setting a member *always* clears the nested
   * values it contains - no matter the state of the current context.
   * If this difference is important, the caller can always detect this case and avoid
   * passing in the context override altogether. */

  if (self->ctx_temp.win_is_set) {
    CTX_wm_window_set(C, self->ctx_temp.win);
  }
  if (self->ctx_temp.area_is_set) {
    CTX_wm_area_set(C, self->ctx_temp.area);
  }
  if (self->ctx_temp.region_is_set) {
    CTX_wm_region_set(C, self->ctx_temp.region);
  }

  Py_RETURN_NONE;
}

static PyObject *bpy_rna_context_temp_override_exit(BPyContextTempOverride *self,
                                                    PyObject * /*args*/)
{
  bContext *C = self->context;

  /* Special case where the window is expected to be freed on file-read,
   * in this case the window should not be restored, see: #92818. */
  bool do_restore = true;
  if (self->ctx_init.win) {
    wmWindowManager *wm = CTX_wm_manager(C);
    if (BLI_findindex(&wm->windows, self->ctx_init.win) == -1) {
      CTX_wm_window_set(C, nullptr);
      do_restore = false;
    }
  }

  if (do_restore) {
    /* Restore context members as needed.
     *
     * The checks here behaves as follows:
     * - When `self->ctx_init.win_is_set` is true, the window was changed by the override.
     *   in this case restore the initial window.
     * - When `self->ctx_temp.win_is_set` is true, the window was set to the current value.
     *   Setting the window (even to the current value) must be accounted for
     *   because setting the window clears the area and the region members,
     *   which must now be restored.
     *
     * `is_container_set` is used to detect if nested context members need to be restored.
     * The comments above refer to the window, it also applies to the area which contains a region.
     */
    bool is_container_set = false;

    if (self->ctx_init.win_is_set) {
      CTX_wm_window_set(C, self->ctx_init.win);
      is_container_set = true;
    }
    else if (self->ctx_temp.win_is_set) {
      is_container_set = true;
    }

    if (self->ctx_init.area_is_set || is_container_set) {
      CTX_wm_area_set(C, self->ctx_init.area);
      is_container_set = true;
    }
    else if (self->ctx_temp.area_is_set) {
      is_container_set = true;
    }

    if (self->ctx_init.region_is_set || is_container_set) {
      CTX_wm_region_set(C, self->ctx_init.region);
      is_container_set = true;
    }
    else if (self->ctx_temp.region_is_set) {
      is_container_set = true;
    }
    UNUSED_VARS(is_container_set);
  }

  /* A copy may have been made when writing context members, see #BPY_context_dict_clear_members */
  PyObject *context_dict_test = static_cast<PyObject *>(CTX_py_dict_get(C));
  if (context_dict_test && (context_dict_test != self->py_state_context_dict)) {
    Py_DECREF(context_dict_test);
  }
  CTX_py_state_pop(C, &self->py_state);
  Py_CLEAR(self->py_state_context_dict);

  Py_RETURN_NONE;
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef bpy_rna_context_temp_override__tp_methods[] = {
    {"__enter__", (PyCFunction)bpy_rna_context_temp_override_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_rna_context_temp_override_exit, METH_VARARGS},
    {nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

static PyTypeObject BPyContextTempOverride_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ContextTempOverride",
    /*tp_basicsize*/ sizeof(BPyContextTempOverride),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_rna_context_temp_override__tp_dealloc,
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
    /*tp_methods*/ bpy_rna_context_temp_override__tp_methods,
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Temporary Override Method
 * \{ */

static PyObject *bpy_context_temp_override_extract_known_args(const char *const *kwds_static,
                                                              PyObject *kwds)
{
  PyObject *sentinel = Py_Ellipsis;
  PyObject *kwds_parse = PyDict_New();
  for (int i = 0; kwds_static[i]; i++) {
    PyObject *key = PyUnicode_FromString(kwds_static[i]);
    PyObject *val = _PyDict_Pop(kwds, key, sentinel);
    if (val != sentinel) {
      if (PyDict_SetItem(kwds_parse, key, val) == -1) {
        BLI_assert_unreachable();
      }
    }
    Py_DECREF(key);
    Py_DECREF(val);
  }
  return kwds_parse;
}

PyDoc_STRVAR(bpy_context_temp_override_doc,
             ".. method:: temp_override(window, area, region, **keywords)\n"
             "\n"
             "   Context manager to temporarily override members in the context.\n"
             "\n"
             "   :arg window: Window override or None.\n"
             "   :type window: :class:`bpy.types.Window`\n"
             "   :arg area: Area override or None.\n"
             "   :type area: :class:`bpy.types.Area`\n"
             "   :arg region: Region override or None.\n"
             "   :type region: :class:`bpy.types.Region`\n"
             "   :arg keywords: Additional keywords override context members.\n"
             "   :return: The context manager .\n"
             "   :rtype: context manager\n");
static PyObject *bpy_context_temp_override(PyObject *self, PyObject *args, PyObject *kwds)
{
  const PointerRNA *context_ptr = pyrna_struct_as_ptr(self, &RNA_Context);
  if (context_ptr == nullptr) {
    return nullptr;
  }

  if (kwds == nullptr) {
    /* While this is effectively NOP, support having no keywords as it's more involved
     * to return an alternative (dummy) context manager. */
  }
  else {
    /* Needed because the keywords copied into `kwds_parse` could contain anything.
     * As the types of keys aren't checked. */
    if (!PyArg_ValidateKeywordArguments(kwds)) {
      return nullptr;
    }
  }

  struct {
    BPy_StructRNA_Parse window;
    BPy_StructRNA_Parse area;
    BPy_StructRNA_Parse region;
  } params{};
  params.window.type = &RNA_Window;
  params.area.type = &RNA_Area;
  params.region.type = &RNA_Region;

  static const char *const _keywords[] = {"window", "area", "region", nullptr};
  static _PyArg_Parser _parser = {
      "|$" /* Optional, keyword only arguments. */
      "O&" /* `window` */
      "O&" /* `area` */
      "O&" /* `region` */
      ":temp_override",
      _keywords,
      nullptr,
  };
  /* Parse known keywords, the remaining keywords are set using #CTX_py_state_push. */
  kwds = kwds ? PyDict_Copy(kwds) : PyDict_New();
  {
    PyObject *kwds_parse = bpy_context_temp_override_extract_known_args(_keywords, kwds);
    const int parse_result = _PyArg_ParseTupleAndKeywordsFast(args,
                                                              kwds_parse,
                                                              &_parser,
                                                              pyrna_struct_as_ptr_or_null_parse,
                                                              &params.window,
                                                              pyrna_struct_as_ptr_or_null_parse,
                                                              &params.area,
                                                              pyrna_struct_as_ptr_or_null_parse,
                                                              &params.region);
    Py_DECREF(kwds_parse);
    if (parse_result == -1) {
      Py_DECREF(kwds);
      return nullptr;
    }
  }

  bContext *C = static_cast<bContext *>(context_ptr->data);
  {
    /* Merge existing keys that don't exist in the keywords passed in.
     * This makes it possible to nest context overrides. */
    PyObject *context_dict_current = static_cast<PyObject *>(CTX_py_dict_get(C));
    if (context_dict_current != nullptr) {
      PyDict_Merge(kwds, context_dict_current, 0);
    }
  }

  ContextStore ctx_temp = {nullptr};
  if (params.window.ptr != nullptr) {
    ctx_temp.win = static_cast<wmWindow *>(params.window.ptr->data);
    ctx_temp.win_is_set = true;
  }
  if (params.area.ptr != nullptr) {
    ctx_temp.area = static_cast<ScrArea *>(params.area.ptr->data);
    ctx_temp.area_is_set = true;
  }

  if (params.region.ptr != nullptr) {
    ctx_temp.region = static_cast<ARegion *>(params.region.ptr->data);
    ctx_temp.region_is_set = true;
  }

  BPyContextTempOverride *ret = PyObject_New(BPyContextTempOverride, &BPyContextTempOverride_Type);
  ret->context = C;
  ret->ctx_temp = ctx_temp;
  memset(&ret->ctx_init, 0, sizeof(ret->ctx_init));

  ret->py_state_context_dict = kwds;

  return (PyObject *)ret;
}

/** \} */

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

PyMethodDef BPY_rna_context_temp_override_method_def = {
    "temp_override",
    (PyCFunction)bpy_context_temp_override,
    METH_VARARGS | METH_KEYWORDS,
    bpy_context_temp_override_doc,
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

void bpy_rna_context_types_init()
{
  if (PyType_Ready(&BPyContextTempOverride_Type) < 0) {
    BLI_assert_unreachable();
    return;
  }
}
