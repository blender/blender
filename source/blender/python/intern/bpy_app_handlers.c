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
 * \ingroup pythonintern
 *
 * This file defines a 'PyStructSequence' accessed via 'bpy.app.handlers',
 * which exposes various lists that the script author can add callback
 * functions into (called via blenders generic BLI_cb api)
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "BKE_callbacks.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "bpy_app_handlers.h"
#include "bpy_rna.h"

#include "../generic/python_utildefines.h"

#include "BPY_extern.h"

void bpy_app_generic_callback(struct Main *main,
                              struct PointerRNA **pointers,
                              const int num_pointers,
                              void *arg);

static PyTypeObject BlenderAppCbType;

static PyStructSequence_Field app_cb_info_fields[] = {
    {"frame_change_pre",
     "Called after frame change for playback and rendering, before any data is evaluated for the "
     "new frame. This makes it possible to change data and relations (for example swap an object "
     "to another mesh) for the new frame. Note that this handler is **not** to be used as 'before "
     "the frame changes' event. The dependency graph is not available in this handler, as data "
     "and relations may have been altered and the dependency graph has not yet been updated for "
     "that."},
    {"frame_change_post",
     "Called after frame change for playback and rendering, after the data has been evaluated "
     "for the new frame."},
    {"render_pre", "on render (before)"},
    {"render_post", "on render (after)"},
    {"render_write", "on writing a render frame (directly after the frame is written)"},
    {"render_stats", "on printing render statistics"},
    {"render_init", "on initialization of a render job"},
    {"render_complete", "on completion of render job"},
    {"render_cancel", "on canceling a render job"},
    {"load_pre", "on loading a new blend file (before)"},
    {"load_post", "on loading a new blend file (after)"},
    {"save_pre", "on saving a blend file (before)"},
    {"save_post", "on saving a blend file (after)"},
    {"undo_pre", "on loading an undo step (before)"},
    {"undo_post", "on loading an undo step (after)"},
    {"redo_pre", "on loading a redo step (before)"},
    {"redo_post", "on loading a redo step (after)"},
    {"depsgraph_update_pre", "on depsgraph update (pre)"},
    {"depsgraph_update_post", "on depsgraph update (post)"},
    {"version_update", "on ending the versioning code"},
    {"load_factory_preferences_post", "on loading factory preferences (after)"},
    {"load_factory_startup_post", "on loading factory startup (after)"},

/* sets the permanent tag */
#define APP_CB_OTHER_FIELDS 1
    {"persistent",
     "Function decorator for callback functions not to be removed when loading new files"},

    {NULL},
};

static PyStructSequence_Desc app_cb_info_desc = {
    "bpy.app.handlers",                    /* name */
    "This module contains callback lists", /* doc */
    app_cb_info_fields,                    /* fields */
    ARRAY_SIZE(app_cb_info_fields) - 1,
};

#if 0
#  if (BKE_CB_EVT_TOT != ARRAY_SIZE(app_cb_info_fields))
#    error "Callbacks are out of sync"
#  endif
#endif

/* --------------------------------------------------------------------------*/
/* permanent tagging code */
#define PERMINENT_CB_ID "_bpy_persistent"

static PyObject *bpy_app_handlers_persistent_new(PyTypeObject *UNUSED(type),
                                                 PyObject *args,
                                                 PyObject *UNUSED(kwds))
{
  PyObject *value;

  if (!PyArg_ParseTuple(args, "O:bpy.app.handlers.persistent", &value)) {
    return NULL;
  }

  if (PyFunction_Check(value)) {
    PyObject **dict_ptr = _PyObject_GetDictPtr(value);
    if (dict_ptr == NULL) {
      PyErr_SetString(PyExc_ValueError,
                      "bpy.app.handlers.persistent wasn't able to "
                      "get the dictionary from the function passed");
      return NULL;
    }
    else {
      /* set id */
      if (*dict_ptr == NULL) {
        *dict_ptr = PyDict_New();
      }

      PyDict_SetItemString(*dict_ptr, PERMINENT_CB_ID, Py_None);
    }

    Py_INCREF(value);
    return value;
  }
  else {
    PyErr_SetString(PyExc_ValueError, "bpy.app.handlers.persistent expected a function");
    return NULL;
  }
}

/* dummy type because decorators can't be PyCFunctions */
static PyTypeObject BPyPersistent_Type = {

#if defined(_MSC_VER)
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif

        "persistent", /* tp_name */
    0,                /* tp_basicsize */
    0,                /* tp_itemsize */
    /* methods */
    0,                                                             /* tp_dealloc */
    0,                                                             /* tp_print */
    0,                                                             /* tp_getattr */
    0,                                                             /* tp_setattr */
    0,                                                             /* tp_reserved */
    0,                                                             /* tp_repr */
    0,                                                             /* tp_as_number */
    0,                                                             /* tp_as_sequence */
    0,                                                             /* tp_as_mapping */
    0,                                                             /* tp_hash */
    0,                                                             /* tp_call */
    0,                                                             /* tp_str */
    0,                                                             /* tp_getattro */
    0,                                                             /* tp_setattro */
    0,                                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE, /* tp_flags */
    0,                                                             /* tp_doc */
    0,                                                             /* tp_traverse */
    0,                                                             /* tp_clear */
    0,                                                             /* tp_richcompare */
    0,                                                             /* tp_weaklistoffset */
    0,                                                             /* tp_iter */
    0,                                                             /* tp_iternext */
    0,                                                             /* tp_methods */
    0,                                                             /* tp_members */
    0,                                                             /* tp_getset */
    0,                                                             /* tp_base */
    0,                                                             /* tp_dict */
    0,                                                             /* tp_descr_get */
    0,                                                             /* tp_descr_set */
    0,                                                             /* tp_dictoffset */
    0,                                                             /* tp_init */
    0,                                                             /* tp_alloc */
    bpy_app_handlers_persistent_new,                               /* tp_new */
    0,                                                             /* tp_free */
};

static PyObject *py_cb_array[BKE_CB_EVT_TOT] = {NULL};

static PyObject *make_app_cb_info(void)
{
  PyObject *app_cb_info;
  int pos;

  app_cb_info = PyStructSequence_New(&BlenderAppCbType);
  if (app_cb_info == NULL) {
    return NULL;
  }

  for (pos = 0; pos < BKE_CB_EVT_TOT; pos++) {
    if (app_cb_info_fields[pos].name == NULL) {
      Py_FatalError("invalid callback slots 1");
    }
    PyStructSequence_SET_ITEM(app_cb_info, pos, (py_cb_array[pos] = PyList_New(0)));
  }
  if (app_cb_info_fields[pos + APP_CB_OTHER_FIELDS].name != NULL) {
    Py_FatalError("invalid callback slots 2");
  }

  /* custom function */
  PyStructSequence_SET_ITEM(app_cb_info, pos++, (PyObject *)&BPyPersistent_Type);

  return app_cb_info;
}

PyObject *BPY_app_handlers_struct(void)
{
  PyObject *ret;

#if defined(_MSC_VER)
  BPyPersistent_Type.ob_base.ob_base.ob_type = &PyType_Type;
#endif

  if (PyType_Ready(&BPyPersistent_Type) < 0) {
    BLI_assert(!"error initializing 'bpy.app.handlers.persistent'");
  }

  PyStructSequence_InitType(&BlenderAppCbType, &app_cb_info_desc);

  ret = make_app_cb_info();

  /* prevent user from creating new instances */
  BlenderAppCbType.tp_init = NULL;
  BlenderAppCbType.tp_new = NULL;
  BlenderAppCbType.tp_hash = (hashfunc)
      _Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

  /* assign the C callbacks */
  if (ret) {
    static bCallbackFuncStore funcstore_array[BKE_CB_EVT_TOT] = {{NULL}};
    bCallbackFuncStore *funcstore;
    int pos = 0;

    for (pos = 0; pos < BKE_CB_EVT_TOT; pos++) {
      funcstore = &funcstore_array[pos];
      funcstore->func = bpy_app_generic_callback;
      funcstore->alloc = 0;
      funcstore->arg = POINTER_FROM_INT(pos);
      BKE_callback_add(funcstore, pos);
    }
  }

  return ret;
}

void BPY_app_handlers_reset(const short do_all)
{
  PyGILState_STATE gilstate;
  int pos = 0;

  gilstate = PyGILState_Ensure();

  if (do_all) {
    for (pos = 0; pos < BKE_CB_EVT_TOT; pos++) {
      /* clear list */
      PyList_SetSlice(py_cb_array[pos], 0, PY_SSIZE_T_MAX, NULL);
    }
  }
  else {
    /* save string conversion thrashing */
    PyObject *perm_id_str = PyUnicode_FromString(PERMINENT_CB_ID);

    for (pos = 0; pos < BKE_CB_EVT_TOT; pos++) {
      /* clear only items without PERMINENT_CB_ID */
      PyObject *ls = py_cb_array[pos];
      Py_ssize_t i;

      PyObject *item;
      PyObject **dict_ptr;

      for (i = PyList_GET_SIZE(ls) - 1; i >= 0; i--) {

        if ((PyFunction_Check((item = PyList_GET_ITEM(ls, i)))) &&
            (dict_ptr = _PyObject_GetDictPtr(item)) && (*dict_ptr) &&
            (PyDict_GetItem(*dict_ptr, perm_id_str) != NULL)) {
          /* keep */
        }
        else {
          /* remove */
          /* PySequence_DelItem(ls, i); */ /* more obvious buw slower */
          PyList_SetSlice(ls, i, i + 1, NULL);
        }
      }
    }

    Py_DECREF(perm_id_str);
  }

  PyGILState_Release(gilstate);
}

static PyObject *choose_arguments(PyObject *func, PyObject *args_all, PyObject *args_single)
{
  if (!PyFunction_Check(func)) {
    return args_all;
  }
  PyCodeObject *code = (PyCodeObject *)PyFunction_GetCode(func);
  if (code->co_argcount == 1) {
    return args_single;
  }
  return args_all;
}

/* the actual callback - not necessarily called from py */
void bpy_app_generic_callback(struct Main *UNUSED(main),
                              struct PointerRNA **pointers,
                              const int num_pointers,
                              void *arg)
{
  PyObject *cb_list = py_cb_array[POINTER_AS_INT(arg)];
  if (PyList_GET_SIZE(cb_list) > 0) {
    PyGILState_STATE gilstate = PyGILState_Ensure();

    const int num_arguments = 2;
    PyObject *args_all = PyTuple_New(num_arguments); /* save python creating each call */
    PyObject *args_single = PyTuple_New(1);
    PyObject *func;
    PyObject *ret;
    Py_ssize_t pos;

    /* setup arguments */
    for (int i = 0; i < num_pointers; ++i) {
      PyTuple_SET_ITEM(args_all, i, pyrna_struct_CreatePyObject(pointers[i]));
    }
    for (int i = num_pointers; i < num_arguments; ++i) {
      PyTuple_SET_ITEM(args_all, i, Py_INCREF_RET(Py_None));
    }

    if (num_pointers == 0) {
      PyTuple_SET_ITEM(args_single, 0, Py_INCREF_RET(Py_None));
    }
    else {
      PyTuple_SET_ITEM(args_single, 0, pyrna_struct_CreatePyObject(pointers[0]));
    }

    /* Iterate the list and run the callbacks
     * note: don't store the list size since the scripts may remove themselves */
    for (pos = 0; pos < PyList_GET_SIZE(cb_list); pos++) {
      func = PyList_GET_ITEM(cb_list, pos);
      PyObject *args = choose_arguments(func, args_all, args_single);
      ret = PyObject_Call(func, args, NULL);
      if (ret == NULL) {
        /* Don't set last system variables because they might cause some
         * dangling pointers to external render engines (when exception
         * happens during rendering) which will break logic of render pipeline
         * which expects to be the only user of render engine when rendering
         * is finished.
         */
        PyErr_PrintEx(0);
        PyErr_Clear();
      }
      else {
        Py_DECREF(ret);
      }
    }

    Py_DECREF(args_all);
    Py_DECREF(args_single);

    PyGILState_Release(gilstate);
  }
}
