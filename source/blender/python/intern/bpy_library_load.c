/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file exposed blend file library appending/linking to python, typically
 * this would be done via RNA api but in this case a hand written python api
 * allows us to use Python's context manager (`__enter__` and `__exit__`).
 *
 * Everything here is exposed via `bpy.data.libraries.load(...)` which returns
 * a context manager.
 */

#include <Python.h>
#include <stddef.h>

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_blendfile_link_append.h"
#include "BKE_context.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DNA_space_types.h" /* FILE_LINK, FILE_RELPATH */

#include "BLO_readfile.h"

#include "MEM_guardedalloc.h"

#include "bpy_capi_utils.h"
#include "bpy_library.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

/* nifty feature. swap out strings for RNA data */
#define USE_RNA_DATABLOCKS

#ifdef USE_RNA_DATABLOCKS
#  include "RNA_access.h"
#  include "bpy_rna.h"
#endif

typedef struct {
  PyObject_HEAD /* Required Python macro. */
  /* Collection iterator specific parts. */
  char relpath[FILE_MAX];
  char abspath[FILE_MAX]; /* absolute path */
  BlendHandle *blo_handle;
  /* Referenced by `blo_handle`, so stored here to keep alive for long enough. */
  ReportList reports;
  BlendFileReadReport bf_reports;

  int flag;
  PyObject *dict;
  /* Borrowed reference to the `bmain`, taken from the RNA instance of #RNA_BlendDataLibraries.
   * Defaults to #G.main, Otherwise use a temporary #Main when `bmain_is_temp` is true. */
  Main *bmain;
  bool bmain_is_temp;
} BPy_Library;

static PyObject *bpy_lib_load(BPy_PropertyRNA *self, PyObject *args, PyObject *kwds);
static PyObject *bpy_lib_enter(BPy_Library *self);
static PyObject *bpy_lib_exit(BPy_Library *self, PyObject *args);
static PyObject *bpy_lib_dir(BPy_Library *self);

static PyMethodDef bpy_lib_methods[] = {
    {"__enter__", (PyCFunction)bpy_lib_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_lib_exit, METH_VARARGS},
    {"__dir__", (PyCFunction)bpy_lib_dir, METH_NOARGS},
    {NULL} /* sentinel */
};

static void bpy_lib_dealloc(BPy_Library *self)
{
  Py_XDECREF(self->dict);
  Py_TYPE(self)->tp_free(self);
}

static PyTypeObject bpy_lib_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_lib", /* tp_name */
    sizeof(BPy_Library),                      /* tp_basicsize */
    0,                                        /* tp_itemsize */
    /* methods */
    (destructor)bpy_lib_dealloc, /* tp_dealloc */
    0,                           /* tp_vectorcall_offset */
    NULL,                        /* getattrfunc tp_getattr; */
    NULL,                        /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* DEPRECATED in python 3.0! */
    NULL,            /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* Will only use these if this is a sub-type of a Python class. */
    PyObject_GenericGetAttr, /* getattrofunc tp_getattro; */
    NULL,                    /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

    /***  weak reference enabler ***/
    0,
    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    bpy_lib_methods,             /* struct PyMethodDef *tp_methods; */
    NULL,                        /* struct PyMemberDef *tp_members; */
    NULL,                        /* struct PyGetSetDef *tp_getset; */
    NULL,                        /* struct _typeobject *tp_base; */
    NULL,                        /* PyObject *tp_dict; */
    NULL,                        /* descrgetfunc tp_descr_get; */
    NULL,                        /* descrsetfunc tp_descr_set; */
    offsetof(BPy_Library, dict), /* long tp_dictoffset; */
    NULL,                        /* initproc tp_init; */
    NULL,                        /* allocfunc tp_alloc; */
    NULL,                        /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free; */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc; */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro; */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

PyDoc_STRVAR(
    bpy_lib_load_doc,
    ".. method:: load(filepath, link=False, relative=False, assets_only=False)\n"
    "\n"
    "   Returns a context manager which exposes 2 library objects on entering.\n"
    "   Each object has attributes matching bpy.data which are lists of strings to be linked.\n"
    "\n"
    "   :arg filepath: The path to a blend file.\n"
    "   :type filepath: string\n"
    "   :arg link: When False reference to the original file is lost.\n"
    "   :type link: bool\n"
    "   :arg relative: When True the path is stored relative to the open blend file.\n"
    "   :type relative: bool\n"
    "   :arg assets_only: If True, only list data-blocks marked as assets.\n"
    "   :type assets_only: bool\n");
static PyObject *bpy_lib_load(BPy_PropertyRNA *self, PyObject *args, PyObject *kw)
{
  Main *bmain_base = CTX_data_main(BPY_context_get());
  Main *bmain = self->ptr.data; /* Typically #G_MAIN */
  BPy_Library *ret;
  const char *filepath = NULL;
  bool is_rel = false, is_link = false, use_assets_only = false;

  static const char *_keywords[] = {"filepath", "link", "relative", "assets_only", NULL};
  static _PyArg_Parser _parser = {
      "s" /* `filepath` */
      /* Optional keyword only arguments. */
      "|$"
      "O&" /* `link` */
      "O&" /* `relative` */
      "O&" /* `assets_only` */
      ":load",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        &filepath,
                                        PyC_ParseBool,
                                        &is_link,
                                        PyC_ParseBool,
                                        &is_rel,
                                        PyC_ParseBool,
                                        &use_assets_only)) {
    return NULL;
  }

  ret = PyObject_New(BPy_Library, &bpy_lib_Type);

  BLI_strncpy(ret->relpath, filepath, sizeof(ret->relpath));
  BLI_strncpy(ret->abspath, filepath, sizeof(ret->abspath));
  BLI_path_abs(ret->abspath, BKE_main_blendfile_path(bmain));

  ret->bmain = bmain;
  ret->bmain_is_temp = (bmain != bmain_base);

  ret->blo_handle = NULL;
  ret->flag = ((is_link ? FILE_LINK : 0) | (is_rel ? FILE_RELPATH : 0) |
               (use_assets_only ? FILE_ASSETS_ONLY : 0));

  ret->dict = _PyDict_NewPresized(INDEX_ID_MAX);

  return (PyObject *)ret;
}

static PyObject *_bpy_names(BPy_Library *self, int blocktype)
{
  PyObject *list;
  LinkNode *l, *names;
  int totnames;

  names = BLO_blendhandle_get_datablock_names(
      self->blo_handle, blocktype, (self->flag & FILE_ASSETS_ONLY) != 0, &totnames);
  list = PyList_New(totnames);

  if (names) {
    int counter = 0;
    for (l = names; l; l = l->next) {
      PyList_SET_ITEM(list, counter, PyUnicode_FromString((char *)l->link));
      counter++;
    }
    BLI_linklist_freeN(names); /* free linklist *and* each node's data */
  }

  return list;
}

static PyObject *bpy_lib_enter(BPy_Library *self)
{
  PyObject *ret;
  BPy_Library *self_from;
  PyObject *from_dict = _PyDict_NewPresized(INDEX_ID_MAX);
  ReportList *reports = &self->reports;
  BlendFileReadReport *bf_reports = &self->bf_reports;

  BKE_reports_init(reports, RPT_STORE);
  memset(bf_reports, 0, sizeof(*bf_reports));
  bf_reports->reports = reports;

  self->blo_handle = BLO_blendhandle_from_file(self->abspath, bf_reports);

  if (self->blo_handle == NULL) {
    if (BPy_reports_to_error(reports, PyExc_IOError, true) != -1) {
      PyErr_Format(PyExc_IOError, "load: %s failed to open blend file", self->abspath);
    }
    return NULL;
  }

  int i = 0, code;
  while ((code = BKE_idtype_idcode_iter_step(&i))) {
    if (BKE_idtype_idcode_is_linkable(code)) {
      const char *name_plural = BKE_idtype_idcode_to_name_plural(code);
      PyObject *str = PyUnicode_FromString(name_plural);
      PyObject *item;

      PyDict_SetItem(self->dict, str, item = PyList_New(0));
      Py_DECREF(item);
      PyDict_SetItem(from_dict, str, item = _bpy_names(self, code));
      Py_DECREF(item);

      Py_DECREF(str);
    }
  }

  /* create a dummy */
  self_from = PyObject_New(BPy_Library, &bpy_lib_Type);
  BLI_strncpy(self_from->relpath, self->relpath, sizeof(self_from->relpath));
  BLI_strncpy(self_from->abspath, self->abspath, sizeof(self_from->abspath));

  self_from->blo_handle = NULL;
  self_from->flag = 0;
  self_from->dict = from_dict; /* owns the dict */

  /* return pair */
  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, (PyObject *)self_from, (PyObject *)self);
  Py_INCREF(self);

  BKE_reports_clear(reports);

  return ret;
}

static void bpy_lib_exit_warn_idname(BPy_Library *self,
                                     const char *name_plural,
                                     const char *idname)
{
  PyObject *exc, *val, *tb;
  PyErr_Fetch(&exc, &val, &tb);
  if (PyErr_WarnFormat(PyExc_UserWarning,
                       1,
                       "load: '%s' does not contain %s[\"%s\"]",
                       self->abspath,
                       name_plural,
                       idname)) {
    /* Spurious errors can appear at shutdown */
    if (PyErr_ExceptionMatches(PyExc_Warning)) {
      PyErr_WriteUnraisable((PyObject *)self);
    }
  }
  PyErr_Restore(exc, val, tb);
}

static void bpy_lib_exit_warn_type(BPy_Library *self, PyObject *item)
{
  PyObject *exc, *val, *tb;
  PyErr_Fetch(&exc, &val, &tb);
  if (PyErr_WarnFormat(PyExc_UserWarning,
                       1,
                       "load: '%s' expected a string type, not a %.200s",
                       self->abspath,
                       Py_TYPE(item)->tp_name)) {
    /* Spurious errors can appear at shutdown */
    if (PyErr_ExceptionMatches(PyExc_Warning)) {
      PyErr_WriteUnraisable((PyObject *)self);
    }
  }
  PyErr_Restore(exc, val, tb);
}

struct LibExitLappContextItemsIterData {
  short idcode;
  BPy_Library *py_library;
  PyObject *py_list;
  Py_ssize_t py_list_size;
};

static bool bpy_lib_exit_lapp_context_items_cb(BlendfileLinkAppendContext *lapp_context,
                                               BlendfileLinkAppendContextItem *item,
                                               void *userdata)
{
  struct LibExitLappContextItemsIterData *data = userdata;

  /* Since `bpy_lib_exit` loops over all ID types, all items in `lapp_context` end up being looped
   * over for each ID type, so when it does not match the item can simply be skipped: it either has
   * already been processed, or will be processed in a later loop. */
  if (BKE_blendfile_link_append_context_item_idcode_get(lapp_context, item) != data->idcode) {
    return true;
  }

  const int py_list_index = POINTER_AS_INT(
      BKE_blendfile_link_append_context_item_userdata_get(lapp_context, item));
  ID *new_id = BKE_blendfile_link_append_context_item_newid_get(lapp_context, item);

  BLI_assert(py_list_index < data->py_list_size);

  /* Fully invalid items (which got set to `Py_None` already in first loop of `bpy_lib_exit`)
   * should never be accessed here, since their index should never be set to any item in
   * `lapp_context`. */
  PyObject *item_src = PyList_GET_ITEM(data->py_list, py_list_index);
  BLI_assert(item_src != Py_None);

  PyObject *py_item;
  if (new_id != NULL) {
    PointerRNA newid_ptr;
    RNA_id_pointer_create(new_id, &newid_ptr);
    py_item = pyrna_struct_CreatePyObject(&newid_ptr);
  }
  else {
    const char *item_idname = PyUnicode_AsUTF8(item_src);
    const char *idcode_name_plural = BKE_idtype_idcode_to_name_plural(data->idcode);

    bpy_lib_exit_warn_idname(data->py_library, idcode_name_plural, item_idname);

    py_item = Py_INCREF_RET(Py_None);
  }

  PyList_SET_ITEM(data->py_list, py_list_index, py_item);

  Py_DECREF(item_src);

  return true;
}

static PyObject *bpy_lib_exit(BPy_Library *self, PyObject *UNUSED(args))
{
  Main *bmain = self->bmain;
  const bool do_append = ((self->flag & FILE_LINK) == 0);

  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* here appending/linking starts */
  const int id_tag_extra = self->bmain_is_temp ? LIB_TAG_TEMP_MAIN : 0;
  struct LibraryLink_Params liblink_params;
  BLO_library_link_params_init(&liblink_params, bmain, self->flag, id_tag_extra);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(
      &liblink_params);
  BKE_blendfile_link_append_context_library_add(lapp_context, self->abspath, self->blo_handle);

  int idcode_step = 0;
  short idcode;
  while ((idcode = BKE_idtype_idcode_iter_step(&idcode_step))) {
    if (!BKE_idtype_idcode_is_linkable(idcode) || (idcode == ID_WS && !do_append)) {
      continue;
    }

    const char *name_plural = BKE_idtype_idcode_to_name_plural(idcode);
    PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
    // printf("lib: %s\n", name_plural);
    if (ls == NULL || !PyList_Check(ls)) {
      continue;
    }

    const Py_ssize_t size = PyList_GET_SIZE(ls);
    if (size == 0) {
      continue;
    }

    /* loop */
    for (Py_ssize_t i = 0; i < size; i++) {
      PyObject *item_src = PyList_GET_ITEM(ls, i);
      const char *item_idname = PyUnicode_AsUTF8(item_src);

      // printf("  %s\n", item_idname);

      /* NOTE: index of item in py list is stored in userdata pointer, so that it can be found
       * later on to replace the ID name by the actual ID pointer. */
      if (item_idname != NULL) {
        BlendfileLinkAppendContextItem *item = BKE_blendfile_link_append_context_item_add(
            lapp_context, item_idname, idcode, POINTER_FROM_INT(i));
        BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, 0);
      }
      else {
        /* XXX, could complain about this */
        bpy_lib_exit_warn_type(self, item_src);
        PyErr_Clear();

#ifdef USE_RNA_DATABLOCKS
        /* We can replace the item immediately with `None`. */
        PyObject *py_item = Py_INCREF_RET(Py_None);
        PyList_SET_ITEM(ls, i, py_item);
        Py_DECREF(item_src);
#endif
      }
    }
  }

  BKE_blendfile_link(lapp_context, NULL);
  if (do_append) {
    BKE_blendfile_append(lapp_context, NULL);
  }

  /* If enabled, replace named items in given lists by the final matching new ID pointer. */
#ifdef USE_RNA_DATABLOCKS
  idcode_step = 0;
  while ((idcode = BKE_idtype_idcode_iter_step(&idcode_step))) {
    if (!BKE_idtype_idcode_is_linkable(idcode) || (idcode == ID_WS && !do_append)) {
      continue;
    }
    const char *name_plural = BKE_idtype_idcode_to_name_plural(idcode);
    PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
    // printf("lib: %s\n", name_plural);
    if (ls == NULL || !PyList_Check(ls)) {
      continue;
    }

    const Py_ssize_t size = PyList_GET_SIZE(ls);
    if (size == 0) {
      continue;
    }

    /* Loop over linked items in `lapp_context` to find matching python one in the list, and
     * replace them with proper ID pointer. */
    struct LibExitLappContextItemsIterData iter_data = {
        .idcode = idcode, .py_library = self, .py_list = ls, .py_list_size = size};
    BKE_blendfile_link_append_context_item_foreach(
        lapp_context,
        bpy_lib_exit_lapp_context_items_cb,
        BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT,
        &iter_data);
  }
#endif  // USE_RNA_DATABLOCKS

  BLO_blendhandle_close(self->blo_handle);
  self->blo_handle = NULL;

  BKE_blendfile_link_append_context_free(lapp_context);
  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

  Py_RETURN_NONE;
}

static PyObject *bpy_lib_dir(BPy_Library *self)
{
  return PyDict_Keys(self->dict);
}

PyMethodDef BPY_library_load_method_def = {
    "load",
    (PyCFunction)bpy_lib_load,
    METH_VARARGS | METH_KEYWORDS,
    bpy_lib_load_doc,
};

int BPY_library_load_type_ready(void)
{
  if (PyType_Ready(&bpy_lib_Type) < 0) {
    return -1;
  }

  return 0;
}
