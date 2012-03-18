/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_library.c
 *  \ingroup pythonintern
 *
 * This file exposed blend file library appending/linking to python, typically
 * this would be done via RNA api but in this case a hand written python api
 * allows us to use pythons context manager (__enter__ and __exit__).
 *
 * Everything here is exposed via bpy.data.libraries.load(...) which returns
 * a context manager.
 */

/* nifty feature. swap out strings for RNA data */
#define USE_RNA_DATABLOCKS

#include <Python.h>
#include <stddef.h>

#include "BLO_readfile.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_idcode.h"
#include "BKE_report.h"
#include "BKE_context.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_listbase.h"

#include "DNA_space_types.h" /* FILE_LINK, FILE_RELPATH */

#include "bpy_util.h"

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
static PyObject *bpy_lib_enter(BPy_Library *self, PyObject *args);
static PyObject *bpy_lib_exit(BPy_Library *self, PyObject *args);
static PyObject *bpy_lib_dir(BPy_Library *self);

static PyMethodDef bpy_lib_methods[] = {
	{"__enter__", (PyCFunction)bpy_lib_enter, METH_NOARGS},
	{"__exit__",  (PyCFunction)bpy_lib_exit,  METH_VARARGS},
	{"__dir__",   (PyCFunction)bpy_lib_dir,   METH_NOARGS},
	{NULL}           /* sentinel */
};

static void bpy_lib_dealloc(BPy_Library *self)
{
	Py_XDECREF(self->dict);
	Py_TYPE(self)->tp_free(self);
}


static PyTypeObject bpy_lib_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_lib",		/* tp_name */
	sizeof(BPy_Library),			/* tp_basicsize */
	0,							/* tp_itemsize */
	/* methods */
	(destructor)bpy_lib_dealloc,/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,						/* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,						/* tp_repr */

	/* Method suites for standard classes */

	NULL,						/* PyNumberMethods *tp_as_number; */
	NULL,						/* PySequenceMethods *tp_as_sequence; */
	NULL,						/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	NULL /*PyObject_GenericGetAttr is assigned later */,	/* getattrofunc tp_getattro; */
	NULL,						/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,						/*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,
  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,						/* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	bpy_lib_methods,			/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,				      	/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	offsetof(BPy_Library, dict),/* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,						/* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,						/* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

PyDoc_STRVAR(bpy_lib_load_doc,
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
"   :type relative: bool\n"
);
static PyObject *bpy_lib_load(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"filepath", "link", "relative", NULL};
	BPy_Library *ret;
	const char *filename = NULL;
	int is_rel = 0, is_link = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|ii:load", (char **)kwlist, &filename, &is_link, &is_rel))
		return NULL;

	ret = PyObject_New(BPy_Library, &bpy_lib_Type);

	BLI_strncpy(ret->relpath, filename, sizeof(ret->relpath));
	BLI_strncpy(ret->abspath, filename, sizeof(ret->abspath));
	BLI_path_abs(ret->abspath, G.main->name);

	ret->blo_handle = NULL;
	ret->flag = ((is_link ? FILE_LINK : 0) |
	             (is_rel ? FILE_RELPATH : 0));

	ret->dict = PyDict_New();

	return (PyObject *)ret;
}

static PyObject *_bpy_names(BPy_Library *self, int blocktype)
{
	PyObject *list;
	LinkNode *l, *names;
	int totnames;

	names = BLO_blendhandle_get_datablock_names(self->blo_handle, blocktype, &totnames);

	if (names) {
		int counter = 0;
		list = PyList_New(totnames);
		for (l = names; l; l = l->next) {
			PyList_SET_ITEM(list, counter, PyUnicode_FromString((char *)l->link));
			counter++;
		}
		BLI_linklist_free(names, free);	/* free linklist *and* each node's data */
	}
	else {
		list = PyList_New(0);
	}

	return list;
}

static PyObject *bpy_lib_enter(BPy_Library *self, PyObject *UNUSED(args))
{
	PyObject *ret;
	BPy_Library *self_from;
	PyObject *from_dict = PyDict_New();
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	self->blo_handle = BLO_blendhandle_from_file(self->abspath, &reports);

	if (self->blo_handle == NULL) {
		if (BPy_reports_to_error(&reports, PyExc_IOError, TRUE) != -1) {
			PyErr_Format(PyExc_IOError,
			             "load: %s failed to open blend file",
			             self->abspath);
		}
		return NULL;
	}
	else {
		int i = 0, code;
		while ((code = BKE_idcode_iter_step(&i))) {
			if (BKE_idcode_is_linkable(code)) {
				const char *name_plural = BKE_idcode_to_name_plural(code);
				PyObject *str = PyUnicode_FromString(name_plural);
				PyDict_SetItem(self->dict, str, PyList_New(0));
				PyDict_SetItem(from_dict, str, _bpy_names(self, code));
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

	PyTuple_SET_ITEM(ret, 0, (PyObject *)self_from);

	PyTuple_SET_ITEM(ret, 1, (PyObject *)self);
	Py_INCREF(self);

	BKE_reports_clear(&reports);

	return ret;
}

static void bpy_lib_exit_warn_idname(BPy_Library *self, const char *name_plural, const char *idname)
{
	PyObject *exc, *val, *tb;
	PyErr_Fetch(&exc, &val, &tb);
	if (PyErr_WarnFormat(PyExc_UserWarning, 1,
	                     "load: '%s' does not contain %s[\"%s\"]",
	                     self->abspath, name_plural, idname)) {
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
	if (PyErr_WarnFormat(PyExc_UserWarning, 1,
	                     "load: '%s' expected a string type, not a %.200s",
	                     self->abspath, Py_TYPE(item)->tp_name)) {
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

	flag_all_listbases_ids(LIB_PRE_EXISTING, 1);

	/* here appending/linking starts */
	mainl = BLO_library_append_begin(bmain, &(self->blo_handle), self->relpath);

	{
		int i = 0, code;
		while ((code = BKE_idcode_iter_step(&i))) {
			if (BKE_idcode_is_linkable(code)) {
				const char *name_plural = BKE_idcode_to_name_plural(code);
				PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
				// printf("lib: %s\n", name_plural);
				if (ls && PyList_Check(ls)) {
					/* loop */
					Py_ssize_t size = PyList_GET_SIZE(ls);
					Py_ssize_t i;
					PyObject *item;
					const char *item_str;

					for (i = 0; i < size; i++) {
						item = PyList_GET_ITEM(ls, i);
						item_str = _PyUnicode_AsString(item);

						// printf("  %s\n", item_str);

						if (item_str) {
							ID *id = BLO_library_append_named_part(mainl, &(self->blo_handle), item_str, code);
							if (id) {
#ifdef USE_RNA_DATABLOCKS
								PointerRNA id_ptr;
								RNA_id_pointer_create(id, &id_ptr);
								Py_DECREF(item);
								item = pyrna_struct_CreatePyObject(&id_ptr);
#endif
							}
							else {
								bpy_lib_exit_warn_idname(self, name_plural, item_str);
								/* just warn for now */
								/* err = -1; */
#ifdef USE_RNA_DATABLOCKS
								item = Py_None;
								Py_INCREF(item);
#endif
							}

							/* ID or None */
						}
						else {
							/* XXX, could complain about this */
							bpy_lib_exit_warn_type(self, item);
							PyErr_Clear();

#ifdef USE_RNA_DATABLOCKS
							item = Py_None;
							Py_INCREF(item);
#endif
						}

#ifdef USE_RNA_DATABLOCKS
						PyList_SET_ITEM(ls, i, item);
#endif
					}
				}
			}
		}
	}

	if (err == -1) {
		/* exception raised above, XXX, this leaks some memory */
		BLO_blendhandle_close(self->blo_handle);
		self->blo_handle = NULL;
		flag_all_listbases_ids(LIB_PRE_EXISTING, 0);
		return NULL;
	}
	else {
		Library *lib = mainl->curlib; /* newly added lib, assign before append end */
		BLO_library_append_end(NULL, mainl, &(self->blo_handle), 0, self->flag);
		BLO_blendhandle_close(self->blo_handle);
		self->blo_handle = NULL;

		{	/* copied from wm_operator.c */
			/* mark all library linked objects to be updated */
			recalc_all_library_objects(G.main);

			/* append, rather than linking */
			if ((self->flag & FILE_LINK) == 0) {
				BKE_library_make_local(bmain, lib, 1);
			}
		}

		flag_all_listbases_ids(LIB_PRE_EXISTING, 0);

		Py_RETURN_NONE;
	}
}

static PyObject *bpy_lib_dir(BPy_Library *self)
{
	return PyDict_Keys(self->dict);
}


int bpy_lib_init(PyObject *mod_par)
{
	static PyMethodDef load_meth = {"load", (PyCFunction)bpy_lib_load,
	                               METH_STATIC|METH_VARARGS|METH_KEYWORDS,
	                               bpy_lib_load_doc};

	PyModule_AddObject(mod_par, "_library_load", PyCFunction_New(&load_meth, NULL));

	/* some compilers don't like accessing this directly, delay assignment */
	bpy_lib_Type.tp_getattro = PyObject_GenericGetAttr;

	if (PyType_Ready(&bpy_lib_Type) < 0)
		return -1;

	return 0;
}
