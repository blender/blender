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
 * This file exposed blend file library appending/linking to python, typically
 * this would be done via RNA api but in this case a hand written python api
 * allows us to use Python's context manager (`__enter__` and `__exit__`).
 *
 * Everything here is exposed via `bpy.data.libraries.load(...)` which returns
 * a context manager.
 */

#include <Python.h>
#include <stddef.h>

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"

#include "BKE_context.h"
#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DNA_space_types.h" /* FILE_LINK, FILE_RELPATH */

#include "BLO_readfile.h"

#include "bpy_capi_utils.h"
#include "bpy_library.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

/* nifty feature. swap out strings for RNA data */
#define USE_RNA_DATABLOCKS

#ifdef USE_RNA_DATABLOCKS
#  include "bpy_rna.h"
#  include "RNA_access.h"
#endif

typedef struct {
  PyObject_HEAD /* required python macro */
      /* collection iterator specific parts */
      char relpath[FILE_MAX];
  char abspath[FILE_MAX]; /* absolute path */
  BlendHandle *blo_handle;
  int flag;
  PyObject *dict;
} BPy_Library;

static PyObject *bpy_lib_load(PyObject *self, PyObject *args, PyObject *kwds);
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
    NULL,                        /* printfunc tp_print; */
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

    /* will only use these if this is a subtype of a py class */
    NULL /*PyObject_GenericGetAttr is assigned later */, /* getattrofunc tp_getattro; */
    NULL,                                                /* setattrofunc tp_setattro; */

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
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

PyDoc_STRVAR(
    bpy_lib_load_doc,
    ".. method:: load(filepath, link=False, relative=False)\n"
    "\n"
    "   Returns a context manager which exposes 2 library objects on entering.\n"
    "   Each object has attributes matching bpy.data which are lists of strings to be linked.\n"
    "\n"
    "   :arg filepath: The path to a blend file.\n"
    "   :type filepath: string\n"
    "   :arg link: When False reference to the original file is lost.\n"
    "   :type link: bool\n"
    "   :arg relative: When True the path is stored relative to the open blend file.\n"
    "   :type relative: bool\n");
static PyObject *bpy_lib_load(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  Main *bmain = CTX_data_main(BPy_GetContext());
  BPy_Library *ret;
  const char *filename = NULL;
  bool is_rel = false, is_link = false;

  static const char *_keywords[] = {"filepath", "link", "relative", NULL};
  static _PyArg_Parser _parser = {"s|O&O&:load", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &filename, PyC_ParseBool, &is_link, PyC_ParseBool, &is_rel)) {
    return NULL;
  }

  ret = PyObject_New(BPy_Library, &bpy_lib_Type);

  BLI_strncpy(ret->relpath, filename, sizeof(ret->relpath));
  BLI_strncpy(ret->abspath, filename, sizeof(ret->abspath));
  BLI_path_abs(ret->abspath, BKE_main_blendfile_path(bmain));

  ret->blo_handle = NULL;
  ret->flag = ((is_link ? FILE_LINK : 0) | (is_rel ? FILE_RELPATH : 0));

  ret->dict = _PyDict_NewPresized(MAX_LIBARRAY);

  return (PyObject *)ret;
}

static PyObject *_bpy_names(BPy_Library *self, int blocktype)
{
  PyObject *list;
  LinkNode *l, *names;
  int totnames;

  names = BLO_blendhandle_get_datablock_names(self->blo_handle, blocktype, &totnames);
  list = PyList_New(totnames);

  if (names) {
    int counter = 0;
    for (l = names; l; l = l->next) {
      PyList_SET_ITEM(list, counter, PyUnicode_FromString((char *)l->link));
      counter++;
    }
    BLI_linklist_free(names, free); /* free linklist *and* each node's data */
  }

  return list;
}

static PyObject *bpy_lib_enter(BPy_Library *self)
{
  PyObject *ret;
  BPy_Library *self_from;
  PyObject *from_dict = _PyDict_NewPresized(MAX_LIBARRAY);
  ReportList reports;

  BKE_reports_init(&reports, RPT_STORE);

  self->blo_handle = BLO_blendhandle_from_file(self->abspath, &reports);

  if (self->blo_handle == NULL) {
    if (BPy_reports_to_error(&reports, PyExc_IOError, true) != -1) {
      PyErr_Format(PyExc_IOError, "load: %s failed to open blend file", self->abspath);
    }
    return NULL;
  }
  else {
    int i = 0, code;
    while ((code = BKE_idcode_iter_step(&i))) {
      if (BKE_idcode_is_linkable(code)) {
        const char *name_plural = BKE_idcode_to_name_plural(code);
        PyObject *str = PyUnicode_FromString(name_plural);
        PyObject *item;

        PyDict_SetItem(self->dict, str, item = PyList_New(0));
        Py_DECREF(item);
        PyDict_SetItem(from_dict, str, item = _bpy_names(self, code));
        Py_DECREF(item);

        Py_DECREF(str);
      }
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

  BKE_reports_clear(&reports);

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

static PyObject *bpy_lib_exit(BPy_Library *self, PyObject *UNUSED(args))
{
  Main *bmain = CTX_data_main(BPy_GetContext());
  Main *mainl = NULL;
  int err = 0;
  const bool do_append = ((self->flag & FILE_LINK) == 0);

  BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);

  /* here appending/linking starts */
  mainl = BLO_library_link_begin(bmain, &(self->blo_handle), self->relpath);

  {
    int idcode_step = 0, idcode;
    while ((idcode = BKE_idcode_iter_step(&idcode_step))) {
      if (BKE_idcode_is_linkable(idcode) && (idcode != ID_WS || do_append)) {
        const char *name_plural = BKE_idcode_to_name_plural(idcode);
        PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
        // printf("lib: %s\n", name_plural);
        if (ls && PyList_Check(ls)) {
          /* loop */
          Py_ssize_t size = PyList_GET_SIZE(ls);
          Py_ssize_t i;

          for (i = 0; i < size; i++) {
            PyObject *item_src = PyList_GET_ITEM(ls, i);
            PyObject *item_dst; /* must be set below */
            const char *item_idname = _PyUnicode_AsString(item_src);

            // printf("  %s\n", item_idname);

            if (item_idname) {
              ID *id = BLO_library_link_named_part(
                  mainl, &(self->blo_handle), idcode, item_idname);
              if (id) {
#ifdef USE_RNA_DATABLOCKS
                /* swap name for pointer to the id */
                item_dst = PyCapsule_New((void *)id, NULL, NULL);
#else
                /* leave as is */
                continue;
#endif
              }
              else {
                bpy_lib_exit_warn_idname(self, name_plural, item_idname);
                /* just warn for now */
                /* err = -1; */
                item_dst = Py_INCREF_RET(Py_None);
              }

              /* ID or None */
            }
            else {
              /* XXX, could complain about this */
              bpy_lib_exit_warn_type(self, item_src);
              PyErr_Clear();
              item_dst = Py_INCREF_RET(Py_None);
            }

            /* item_dst must be new or already incref'd */
            Py_DECREF(item_src);
            PyList_SET_ITEM(ls, i, item_dst);
          }
        }
      }
    }
  }

  if (err == -1) {
    /* exception raised above, XXX, this leaks some memory */
    BLO_blendhandle_close(self->blo_handle);
    self->blo_handle = NULL;
    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);
    return NULL;
  }
  else {
    Library *lib = mainl->curlib; /* newly added lib, assign before append end */
    BLO_library_link_end(mainl, &(self->blo_handle), self->flag, NULL, NULL, NULL, NULL);
    BLO_blendhandle_close(self->blo_handle);
    self->blo_handle = NULL;

    GHash *old_to_new_ids = BLI_ghash_ptr_new(__func__);

    /* copied from wm_operator.c */
    {
      /* mark all library linked objects to be updated */
      BKE_main_lib_objects_recalc_all(bmain);

      /* append, rather than linking */
      if (do_append) {
        BKE_library_make_local(bmain, lib, old_to_new_ids, true, false);
      }
    }

    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

    /* finally swap the capsules for real bpy objects
     * important since BLO_library_append_end initializes NodeTree types used by srna->refine */
#ifdef USE_RNA_DATABLOCKS
    {
      int idcode_step = 0, idcode;
      while ((idcode = BKE_idcode_iter_step(&idcode_step))) {
        if (BKE_idcode_is_linkable(idcode) && (idcode != ID_WS || do_append)) {
          const char *name_plural = BKE_idcode_to_name_plural(idcode);
          PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
          if (ls && PyList_Check(ls)) {
            Py_ssize_t size = PyList_GET_SIZE(ls);
            Py_ssize_t i;
            PyObject *item;

            for (i = 0; i < size; i++) {
              item = PyList_GET_ITEM(ls, i);
              if (PyCapsule_CheckExact(item)) {
                PointerRNA id_ptr;
                ID *id;

                id = PyCapsule_GetPointer(item, NULL);
                id = BLI_ghash_lookup_default(old_to_new_ids, id, id);
                Py_DECREF(item);

                RNA_id_pointer_create(id, &id_ptr);
                item = pyrna_struct_CreatePyObject(&id_ptr);
                PyList_SET_ITEM(ls, i, item);
              }
            }
          }
        }
      }
    }
#endif /* USE_RNA_DATABLOCKS */

    BLI_ghash_free(old_to_new_ids, NULL, NULL);
    Py_RETURN_NONE;
  }
}

static PyObject *bpy_lib_dir(BPy_Library *self)
{
  return PyDict_Keys(self->dict);
}

int BPY_library_load_module(PyObject *mod_par)
{
  static PyMethodDef load_meth = {
      "load",
      (PyCFunction)bpy_lib_load,
      METH_STATIC | METH_VARARGS | METH_KEYWORDS,
      bpy_lib_load_doc,
  };
  PyModule_AddObject(mod_par, "_library_load", PyCFunction_New(&load_meth, NULL));

  /* some compilers don't like accessing this directly, delay assignment */
  bpy_lib_Type.tp_getattro = PyObject_GenericGetAttr;

  if (PyType_Ready(&bpy_lib_Type) < 0) {
    return -1;
  }

  return 0;
}
