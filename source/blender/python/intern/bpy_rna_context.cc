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

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bpy_rna_context.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "bpy_rna.hh"

/* -------------------------------------------------------------------- */
/** \name Private Utility Functions
 * \{ */

static void bpy_rna_context_temp_set_screen_for_window(bContext *C, wmWindow *win, bScreen *screen)
{
  if (screen == nullptr) {
    return;
  }
  if (screen == WM_window_get_active_screen(win)) {
    return;
  }

  WorkSpace *workspace;
  BKE_workspace_layout_find_global(CTX_data_main(C), screen, &workspace);
  /* Changing workspace instead of just screen as they are tied. */
  WM_window_set_active_workspace(C, win, workspace);
  WM_window_set_active_screen(win, workspace, screen);
}

/**
 * Switching to or away from this screen is not supported.
 */
static bool wm_check_screen_switch_supported(const bScreen *screen)
{
  if (screen->temp != 0) {
    return false;
  }
  if (BKE_screen_is_fullscreen_area(screen)) {
    return false;
  }
  return true;
}

static bool wm_check_window_exists(const Main *bmain, const wmWindow *win)
{
  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    if (BLI_findindex(&wm->windows, win) != -1) {
      return true;
    }
  }
  return false;
}

static bool wm_check_screen_exists(const Main *bmain, const bScreen *screen)
{
  if (BLI_findindex(&bmain->screens, screen) != -1) {
    return true;
  }
  return false;
}

static bool wm_check_area_exists(const wmWindow *win, const bScreen *screen, const ScrArea *area)
{
  if (win && (BLI_findindex(&win->global_areas.areabase, area) != -1)) {
    return true;
  }
  if (screen && (BLI_findindex(&screen->areabase, area) != -1)) {
    return true;
  }
  return false;
}

static bool wm_check_region_exists(const bScreen *screen,
                                   const ScrArea *area,
                                   const ARegion *region)
{
  if (screen && (BLI_findindex(&screen->regionbase, region) != -1)) {
    return true;
  }
  if (area && (BLI_findindex(&area->regionbase, region) != -1)) {
    return true;
  }
  return false;
}

/**
 * Helper function to configure context logging with extensible options.
 */
static void bpy_rna_context_logging_set(bContext *C, bool enable)
{
  CTX_member_logging_set(C, enable);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Temporary Context Override (Python Context Manager)
 * \{ */

struct ContextStore {
  wmWindow *win;
  bool win_is_set;
  bScreen *screen;
  bool screen_is_set;
  ScrArea *area;
  bool area_is_set;
  ARegion *region;
  bool region_is_set;

  /** User's desired logging state for this temp_override instance (can be changed at runtime). */
  bool use_logging;
};

struct BPyContextTempOverride {
  PyObject_HEAD /* Required Python macro. */
  bContext *context;

  ContextStore ctx_init;
  ContextStore ctx_temp;

  struct {
    /**
     * The original screen of `ctx_temp.win`, needed when restoring this windows screen as it
     * won't be `ctx_init.screen` (when switching the window as well as the screen), see #115937.
     */
    bScreen *screen;
  } ctx_temp_orig;

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

static void bpy_rna_context_temp_override_dealloc(BPyContextTempOverride *self)
{
  PyObject_GC_UnTrack(self);
  Py_CLEAR(self->py_state_context_dict);
  PyObject_GC_Del(self);
}

static int bpy_rna_context_temp_override_traverse(BPyContextTempOverride *self,
                                                  visitproc visit,
                                                  void *arg)
{
  Py_VISIT(self->py_state_context_dict);
  return 0;
}

static int bpy_rna_context_temp_override_clear(BPyContextTempOverride *self)
{
  Py_CLEAR(self->py_state_context_dict);
  return 0;
}

static bool bpy_rna_context_temp_override_enter_ok_or_error(const BPyContextTempOverride *self,
                                                            const Main *bmain,
                                                            const wmWindow *win,
                                                            const bScreen *screen,
                                                            const ScrArea *area,
                                                            const ARegion *region)
{

  /* NOTE(@ideasman42): Regarding sanity checks.
   * There are 3 different situations to be accounted for here regarding overriding windowing data.
   *
   * - 1) Nothing is overridden.
   *   Simple, no sanity checks needed.
   *
   * - 2) Some members are overridden.
   *   Check the state is consistent (that the region is part the area or screen for example).
   *
   * - 3) Some members are overridden *but* the context members are unchanged.
   *   This is a less obvious case which often happens when a Python script copies the context
   *   typically via `context.copy()`, manipulates it and passes it in as keyword arguments.
   *
   *   A naive approach could be to behave as if these arguments weren't passed in
   *   which would work in many situations however there is a difference
   *   since these members are used to restore the context afterwards.
   *   It's possible a script might use this context-manager to *pin* the context,
   *   running actions that change the context, relying on the context to be restored.
   *
   *   When error-checking unchanged context members some error checks must be skipped
   *   such as the check to disallow temporary screens since that could break using
   *   `temp_override(..)` running with the current context from a render-window for example.
   *
   *   In fact all sanity checks could be disabled when the members involved remain unchanged
   *   however it's possible Python scripts corrupt Blender's internal windowing state so keeping
   *   the checks is harmless and alerts developers early on that something is wrong.
   */

  if (self->ctx_temp.region_is_set && (region != nullptr)) {
    if (screen == nullptr && area == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Region set with screen & area set to None");
      return false;
    }
    if (!wm_check_region_exists(screen, area, region)) {
      PyErr_SetString(PyExc_TypeError, "Region not found in area or screen");
      return false;
    }
  }

  if (self->ctx_temp.area_is_set && (area != nullptr)) {
    if (win == nullptr && screen == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Area set with window & screen set to None");
      return false;
    }
    if (!wm_check_area_exists(win, screen, area)) {
      PyErr_SetString(PyExc_TypeError, "Area not found in screen");
      return false;
    }
  }

  if (self->ctx_temp.screen_is_set && (screen != nullptr)) {
    if (win == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Screen set with null window");
      return false;
    }
    if (!wm_check_screen_exists(bmain, screen)) {
      PyErr_SetString(PyExc_TypeError, "Screen not found");
      return false;
    }

    /* Skip some checks when the screen is unchanged. */
    if (self->ctx_init.screen_is_set) {
      /* Switching away from a temporary screen isn't supported. */
      if ((self->ctx_init.screen != nullptr) &&
          !wm_check_screen_switch_supported(self->ctx_init.screen))
      {
        PyErr_SetString(PyExc_TypeError,
                        "Overriding context with an active temporary screen isn't supported");
        return false;
      }
      if (!wm_check_screen_switch_supported(screen)) {
        PyErr_SetString(PyExc_TypeError,
                        "Overriding context with temporary screen isn't supported");
        return false;
      }
      if (BKE_workspace_layout_find_global(bmain, screen, nullptr) == nullptr) {
        PyErr_SetString(PyExc_TypeError, "Screen has no workspace");
        return false;
      }

      LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
        LISTBASE_FOREACH (wmWindow *, win_iter, &wm->windows) {
          if (win_iter == win) {
            continue;
          }
          if (screen == WM_window_get_active_screen(win_iter)) {
            PyErr_SetString(PyExc_TypeError, "Screen is used by another window");
            return false;
          }
        }
      }
    }
  }

  if (self->ctx_temp.win_is_set && (win != nullptr)) {
    if (!wm_check_window_exists(bmain, win)) {
      PyErr_SetString(PyExc_TypeError, "Window not found");
      return false;
    }
  }

  return true;
}

static PyObject *bpy_rna_context_temp_override_enter(BPyContextTempOverride *self)
{
  bContext *C = self->context;
  Main *bmain = CTX_data_main(C);

  /* Enable logging for this temporary override context if the user has requested it. */
  if (self->ctx_temp.use_logging) {
    bpy_rna_context_logging_set(C, true);
  }

  /* It's crucial to call #CTX_py_state_pop if this function fails with an error. */
  CTX_py_state_push(C, &self->py_state, self->py_state_context_dict);

  self->ctx_init.win = CTX_wm_window(C);
  self->ctx_init.screen = self->ctx_init.win ? WM_window_get_active_screen(self->ctx_init.win) :
                                               CTX_wm_screen(C);
  self->ctx_init.area = CTX_wm_area(C);
  self->ctx_init.region = CTX_wm_region(C);

  wmWindow *win = self->ctx_temp.win_is_set ? self->ctx_temp.win : self->ctx_init.win;
  bScreen *screen = self->ctx_temp.screen_is_set ? self->ctx_temp.screen : self->ctx_init.screen;
  ScrArea *area = self->ctx_temp.area_is_set ? self->ctx_temp.area : self->ctx_init.area;
  ARegion *region = self->ctx_temp.region_is_set ? self->ctx_temp.region : self->ctx_init.region;

  self->ctx_init.win_is_set = (self->ctx_init.win != win);
  self->ctx_init.screen_is_set = (self->ctx_init.screen != screen);
  self->ctx_init.area_is_set = (self->ctx_init.area != area);
  self->ctx_init.region_is_set = (self->ctx_init.region != region);

  /* When the screen isn't passed but a window is, match the screen to the window,
   * it's important to do this after setting `self->ctx_init.screen_is_set` because the screen is
   * *not* set, only the window, restoring the window will also restore its screen, see #116297. */
  if ((self->ctx_temp.win_is_set == true) && (self->ctx_temp.screen_is_set == false)) {
    screen = win ? WM_window_get_active_screen(win) : nullptr;
  }

  if (!bpy_rna_context_temp_override_enter_ok_or_error(self, bmain, win, screen, area, region)) {
    CTX_py_state_pop(C, &self->py_state);
    return nullptr;
  }

  /* Manipulate the context (setup). */
  if (self->ctx_temp.screen_is_set) {
    self->ctx_temp_orig.screen = WM_window_get_active_screen(win);
    bpy_rna_context_temp_set_screen_for_window(C, win, self->ctx_temp.screen);
  }

  /* NOTE: always set these members, even when they are equal to the current values because
   * setting the window (for example) clears the area & region, setting the area clears the region.
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
  if (self->ctx_temp.screen_is_set) {
    CTX_wm_screen_set(C, self->ctx_temp.screen);
  }
  if (self->ctx_temp.area_is_set) {
    CTX_wm_area_set(C, self->ctx_temp.area);
  }
  if (self->ctx_temp.region_is_set) {
    CTX_wm_region_set(C, self->ctx_temp.region);
  }

  return Py_NewRef(self);
}

static PyObject *bpy_rna_context_temp_override_exit(BPyContextTempOverride *self,
                                                    PyObject * /*args*/)
{
  bContext *C = self->context;

  Main *bmain = CTX_data_main(C);

  /* Manipulate the context (restore). */
  if (self->ctx_temp.screen_is_set) {
    if (self->ctx_temp_orig.screen && wm_check_screen_exists(bmain, self->ctx_temp_orig.screen)) {
      wmWindow *win = self->ctx_temp.win_is_set ? self->ctx_temp.win : self->ctx_init.win;
      if (win && wm_check_window_exists(bmain, win)) {
        /* Disallow switching away from temporary-screens & full-screen areas, while it could be
         * useful to support this, closing screens uses different and more involved logic
         * compared with switching between user managed screens, see: #117188. */
        if (wm_check_screen_switch_supported(WM_window_get_active_screen(win))) {
          bpy_rna_context_temp_set_screen_for_window(C, win, self->ctx_temp_orig.screen);
        }
      }
    }
  }

  /* Account for the window to be freed on file-read,
   * in this case the window should not be restored, see: #92818.
   * Also account for other windowing members to be removed on exit,
   * in this case the context is cleared. */
  bool do_restore = true;

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
   * The comments above refer to the window, it also applies to the screen containing an area
   * and area which contains a region. */
  bool is_container_set = false;

  /* Handle Window. */
  if (do_restore) {
    if (self->ctx_init.win && !wm_check_window_exists(bmain, self->ctx_init.win)) {
      CTX_wm_window_set(C, nullptr);
      do_restore = false;
    }

    if (do_restore) {
      if (self->ctx_init.win_is_set) {
        CTX_wm_window_set(C, self->ctx_init.win);
        is_container_set = true;
      }
      else if (self->ctx_temp.win_is_set) {
        if (self->ctx_init.win == CTX_wm_window(C)) {
          is_container_set = true;
        }
        else {
          /* If the context changed, it's incorrect to attempt to restored nested members,
           * in this case leave the context as-is, see: #119202. */
          do_restore = false;
        }
      }
    }
  }

  /* Handle Screen. */
  if (do_restore) {
    if (self->ctx_init.screen && !wm_check_screen_exists(bmain, self->ctx_init.screen)) {
      CTX_wm_screen_set(C, nullptr);
      do_restore = false;
    }

    if (do_restore) {
      if (self->ctx_init.screen_is_set || is_container_set) {
        CTX_wm_screen_set(C, self->ctx_init.screen);
        is_container_set = true;
      }
      else if (self->ctx_temp.screen_is_set) {
        if (self->ctx_init.screen == CTX_wm_screen(C)) {
          is_container_set = true;
        }
        else {
          do_restore = false;
        }
      }
    }
  }

  /* Handle Area. */
  if (do_restore) {
    if (self->ctx_init.area &&
        !wm_check_area_exists(self->ctx_init.win, self->ctx_init.screen, self->ctx_init.area))
    {
      CTX_wm_area_set(C, nullptr);
      do_restore = false;
    }

    if (do_restore) {
      if (self->ctx_init.area_is_set || is_container_set) {
        CTX_wm_area_set(C, self->ctx_init.area);
        is_container_set = true;
      }
      else if (self->ctx_temp.area_is_set) {
        if (self->ctx_init.area == CTX_wm_area(C)) {
          is_container_set = true;
        }
        else {
          do_restore = false;
        }
      }
    }
  }

  /* Handle Region. */
  if (do_restore) {
    if (self->ctx_init.region &&
        !wm_check_region_exists(self->ctx_init.screen, self->ctx_init.area, self->ctx_init.region))
    {
      CTX_wm_region_set(C, nullptr);
      do_restore = false;
    }

    if (do_restore) {
      if (self->ctx_init.region_is_set || is_container_set) {
        CTX_wm_region_set(C, self->ctx_init.region);
        is_container_set = true;
      }
      /* Enable is there is ever data nested within the region. */
      else if (false && self->ctx_temp.region_is_set) {
        if (self->ctx_init.region == CTX_wm_region(C)) {
          is_container_set = true;
        }
        else {
          do_restore = false;
        }
      }
    }
  }
  UNUSED_VARS(is_container_set, do_restore);

  /* Finished restoring the context. */

  /* A copy may have been made when writing context members, see #BPY_context_dict_clear_members */
  PyObject *context_dict_test = static_cast<PyObject *>(CTX_py_dict_get(C));
  if (context_dict_test && (context_dict_test != self->py_state_context_dict)) {
    Py_DECREF(context_dict_test);
  }

  /* Restore logging state based on the user's preference stored in ctx_init.use_logging. */
  bpy_rna_context_logging_set(C, self->ctx_init.use_logging);

  CTX_py_state_pop(C, &self->py_state);

  Py_RETURN_NONE;
}

static PyObject *bpy_rna_context_temp_override_logging_set(BPyContextTempOverride *self,
                                                           PyObject *args,
                                                           PyObject *kwds)
{
  bool enable = true;

  static const char *kwlist[] = {"", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&", (char **)kwlist, PyC_ParseBool, &enable)) {
    return nullptr;
  }

  self->ctx_temp.use_logging = enable;

  bpy_rna_context_logging_set(self->context, enable);

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

static PyMethodDef bpy_rna_context_temp_override_methods[] = {
    {"__enter__", (PyCFunction)bpy_rna_context_temp_override_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_rna_context_temp_override_exit, METH_VARARGS},
    {"logging_set",
     (PyCFunction)bpy_rna_context_temp_override_logging_set,
     METH_VARARGS | METH_KEYWORDS},
    {nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyTypeObject BPyContextTempOverride_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ContextTempOverride",
    /*tp_basicsize*/ sizeof(BPyContextTempOverride),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_rna_context_temp_override_dealloc,
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
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ (traverseproc)bpy_rna_context_temp_override_traverse,
    /*tp_clear*/ (inquiry)bpy_rna_context_temp_override_clear,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ bpy_rna_context_temp_override_methods,
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
  PyObject *kwds_parse = PyDict_New();
  for (int i = 0; kwds_static[i]; i++) {
    PyObject *key = PyUnicode_FromString(kwds_static[i]);
    PyObject *val;

#if PY_VERSION_HEX >= 0x030d0000
    switch (PyDict_Pop(kwds, key, &val)) {
      case 1: {
        if (PyDict_SetItem(kwds_parse, key, val) == -1) {
          BLI_assert_unreachable();
        }
        Py_DECREF(val);
        break;
      }
      case -1: {
        /* Not expected, but allow for an error. */
        BLI_assert(false);
        PyErr_Clear();
        break;
      }
    }
#else /* Remove when Python 3.12 support is dropped. */
    PyObject *sentinel = Py_Ellipsis;
    val = _PyDict_Pop(kwds, key, sentinel);
    if (val != sentinel) {
      if (PyDict_SetItem(kwds_parse, key, val) == -1) {
        BLI_assert_unreachable();
      }
    }
    Py_DECREF(val);
#endif

    Py_DECREF(key);
  }
  return kwds_parse;
}

/* NOTE(@ideasman42): `ContextTempOverride` isn't accessible from (without creating an instance),
 * it should be exposed although it doesn't seem especially important either. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_context_temp_override_doc,
    ".. method:: temp_override(*, window=None, screen=None, area=None, region=None, **keywords)\n"
    "\n"
    "   Context manager to temporarily override members in the context.\n"
    "\n"
    "   :arg window: Window override or None.\n"
    "   :type window: :class:`bpy.types.Window`\n"
    "   :arg screen: Screen override or None.\n"
    "\n"
    "      .. note:: Switching to or away from full-screen areas & temporary screens "
    "isn't supported. Passing in these screens will raise an exception, "
    "actions that leave the context such screens won't restore the prior screen.\n"
    "\n"
    "      .. note:: Changing the screen has wider implications "
    "than other arguments as it will also change the works-space "
    "and potentially the scene (when pinned).\n"
    "\n"
    "   :type screen: :class:`bpy.types.Screen`\n"
    "   :arg area: Area override or None.\n"
    "   :type area: :class:`bpy.types.Area`\n"
    "   :arg region: Region override or None.\n"
    "   :type region: :class:`bpy.types.Region`\n"
    "   :arg keywords: Additional keywords override context members.\n"
    "   :return: The context manager .\n"
    "   :rtype: ContextTempOverride\n");
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
    BPy_StructRNA_Parse screen;
    BPy_StructRNA_Parse area;
    BPy_StructRNA_Parse region;
  } params{};
  params.window.type = &RNA_Window;
  params.screen.type = &RNA_Screen;
  params.area.type = &RNA_Area;
  params.region.type = &RNA_Region;

  static const char *const _keywords[] = {
      "window",
      "screen",
      "area",
      "region",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional, keyword only arguments. */
      "O&" /* `window` */
      "O&" /* `screen` */
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
                                                              &params.screen,
                                                              pyrna_struct_as_ptr_or_null_parse,
                                                              &params.area,
                                                              pyrna_struct_as_ptr_or_null_parse,
                                                              &params.region);
    Py_DECREF(kwds_parse);
    if (!parse_result) {
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

  if (params.screen.ptr != nullptr) {
    ctx_temp.screen = static_cast<bScreen *>(params.screen.ptr->data);
    ctx_temp.screen_is_set = true;
  }

  if (params.area.ptr != nullptr) {
    ctx_temp.area = static_cast<ScrArea *>(params.area.ptr->data);
    ctx_temp.area_is_set = true;
  }

  if (params.region.ptr != nullptr) {
    ctx_temp.region = static_cast<ARegion *>(params.region.ptr->data);
    ctx_temp.region_is_set = true;
  }

  BPyContextTempOverride *ret = PyObject_GC_New(BPyContextTempOverride,
                                                &BPyContextTempOverride_Type);
  ret->context = C;
  ret->ctx_temp = ctx_temp;
  memset(&ret->ctx_init, 0, sizeof(ret->ctx_init));

  ret->ctx_temp_orig.screen = nullptr;

  ret->py_state_context_dict = kwds;

  PyObject_GC_Track(ret);

  return (PyObject *)ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Type Definition
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

PyMethodDef BPY_rna_context_temp_override_method_def = {
    "temp_override",
    (PyCFunction)bpy_context_temp_override,
    METH_VARARGS | METH_KEYWORDS,
    bpy_context_temp_override_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

void bpy_rna_context_types_init()
{
  if (PyType_Ready(&BPyContextTempOverride_Type) < 0) {
    BLI_assert_unreachable();
    return;
  }
}

/** \} */
