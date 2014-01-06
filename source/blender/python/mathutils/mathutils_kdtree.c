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
 * Contributor(s): Dan Eicher, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_kdtree.c
 *  \ingroup mathutils
 *
 * This file defines the 'mathutils.kdtree' module, a general purpose module to access
 * blenders kdtree for 3d spatial lookups.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_kdtree.h"

#include "../generic/py_capi_utils.h"

#include "mathutils.h"
#include "mathutils_kdtree.h"  /* own include */

#include "BLI_strict_flags.h"

typedef struct {
	PyObject_HEAD
	KDTree *obj;
	unsigned int maxsize;
	unsigned int count;
	unsigned int count_balance;  /* size when we last balanced */
} PyKDTree;


/* -------------------------------------------------------------------- */
/* Utility helper functions */

static void kdtree_nearest_to_py_tuple(const KDTreeNearest *nearest, PyObject *py_retval)
{
	BLI_assert(nearest->index >= 0);
	BLI_assert(PyTuple_GET_SIZE(py_retval) == 3);

	PyTuple_SET_ITEM(py_retval, 0, Vector_CreatePyObject((float *)nearest->co, 3, Py_NEW, NULL));
	PyTuple_SET_ITEM(py_retval, 1, PyLong_FromLong(nearest->index));
	PyTuple_SET_ITEM(py_retval, 2, PyFloat_FromDouble(nearest->dist));
}

static PyObject *kdtree_nearest_to_py(const KDTreeNearest *nearest)
{
	PyObject *py_retval;

	py_retval = PyTuple_New(3);

	kdtree_nearest_to_py_tuple(nearest, py_retval);

	return py_retval;
}

static PyObject *kdtree_nearest_to_py_and_check(const KDTreeNearest *nearest)
{
	PyObject *py_retval;

	py_retval = PyTuple_New(3);

	if (nearest->index != -1) {
		kdtree_nearest_to_py_tuple(nearest, py_retval);
	}
	else {
		PyC_Tuple_Fill(py_retval, Py_None);
	}

	return py_retval;
}


/* -------------------------------------------------------------------- */
/* KDTree */

/* annoying since arg parsing won't check overflow */
#define UINT_IS_NEG(n) ((n) > INT_MAX)

static int PyKDTree__tp_init(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
	unsigned int maxsize;
	const char *keywords[] = {"size", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, (char *)"I:KDTree", (char **)keywords, &maxsize)) {
		return -1;
	}

	if (UINT_IS_NEG(maxsize)) {
		PyErr_SetString(PyExc_ValueError, "negative 'size' given");
		return -1;
	}

	self->obj = BLI_kdtree_new(maxsize);
	self->maxsize = maxsize;
	self->count = 0;
	self->count_balance = 0;

	return 0;
}

static void PyKDTree__tp_dealloc(PyKDTree *self)
{
	BLI_kdtree_free(self->obj);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(py_kdtree_insert_doc,
".. method:: insert(index, co)\n"
"\n"
"   Insert a point into the KDTree.\n"
"\n"
"   :arg co: Point 3d position.\n"
"   :type co: float triplet\n"
"   :arg index: The index of the point.\n"
"   :type index: int\n"
);
static PyObject *py_kdtree_insert(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_co;
	float co[3];
	int index;
	const char *keywords[] = {"co", "index", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, (char *) "Oi:insert", (char **)keywords,
	                                 &py_co, &index))
	{
		return NULL;
	}

	if (mathutils_array_parse(co, 3, 3, py_co, "insert: invalid 'co' arg") == -1)
		return NULL;

	if (index < 0) {
		PyErr_SetString(PyExc_ValueError, "negative index given");
		return NULL;
	}

	if (self->count >= self->maxsize) {
		PyErr_SetString(PyExc_RuntimeError, "Trying to insert more items than KDTree has room for");
		return NULL;
	}

	BLI_kdtree_insert(self->obj, index, co, NULL);
	self->count++;

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_kdtree_balance_doc,
".. method:: balance()\n"
"\n"
"   Balance the tree.\n"
);
static PyObject *py_kdtree_balance(PyKDTree *self)
{
	BLI_kdtree_balance(self->obj);
	self->count_balance = self->count;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_kdtree_find_doc,
".. method:: find(co)\n"
"\n"
"   Find nearest point to ``co``.\n"
"\n"
"   :arg co: 3d coordinates.\n"
"   :type co: float triplet\n"
"   :return: Returns (:class:`Vector`, index, distance).\n"
"   :rtype: :class:`tuple`\n"
);
static PyObject *py_kdtree_find(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_co;
	float co[3];
	KDTreeNearest nearest;
	const char *keywords[] = {"co", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, (char *) "O:find", (char **)keywords,
	                                 &py_co))
	{
		return NULL;
	}

	if (mathutils_array_parse(co, 3, 3, py_co, "find: invalid 'co' arg") == -1)
		return NULL;

	if (self->count != self->count_balance) {
		PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find()");
		return NULL;
	}


	nearest.index = -1;

	BLI_kdtree_find_nearest(self->obj, co, NULL, &nearest);

	return kdtree_nearest_to_py_and_check(&nearest);
}

PyDoc_STRVAR(py_kdtree_find_n_doc,
".. method:: find_n(co, n)\n"
"\n"
"   Find nearest ``n`` points to ``co``.\n"
"\n"
"   :arg co: 3d coordinates.\n"
"   :type co: float triplet\n"
"   :arg n: Number of points to find.\n"
"   :type n: int\n"
"   :return: Returns a list of tuples (:class:`Vector`, index, distance).\n"
"   :rtype: :class:`list`\n"
);
static PyObject *py_kdtree_find_n(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_list;
	PyObject *py_co;
	float co[3];
	KDTreeNearest *nearest;
	unsigned int n;
	int i, found;
	const char *keywords[] = {"co", "n", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, (char *) "OI:find_n", (char **)keywords,
	                                 &py_co, &n))
	{
		return NULL;
	}

	if (mathutils_array_parse(co, 3, 3, py_co, "find_n: invalid 'co' arg") == -1)
		return NULL;

	if (UINT_IS_NEG(n)) {
		PyErr_SetString(PyExc_RuntimeError, "negative 'n' given");
		return NULL;
	}

	if (self->count != self->count_balance) {
		PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_n()");
		return NULL;
	}

	nearest = MEM_mallocN(sizeof(KDTreeNearest) * n, __func__);

	found = BLI_kdtree_find_nearest_n(self->obj, co, NULL, nearest, n);

	py_list = PyList_New(found);

	for (i = 0; i < found; i++) {
		PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py(&nearest[i]));
	}

	MEM_freeN(nearest);

	return py_list;
}

PyDoc_STRVAR(py_kdtree_find_range_doc,
".. method:: find_range(co, radius)\n"
"\n"
"   Find all points within ``radius`` of ``co``.\n"
"\n"
"   :arg co: 3d coordinates.\n"
"   :type co: float triplet\n"
"   :arg radius: Distance to search for points.\n"
"   :type radius: float\n"
"   :return: Returns a list of tuples (:class:`Vector`, index, distance).\n"
"   :rtype: :class:`list`\n"
);
static PyObject *py_kdtree_find_range(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_list;
	PyObject *py_co;
	float co[3];
	KDTreeNearest *nearest = NULL;
	float radius;
	int i, found;

	const char *keywords[] = {"co", "radius", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, (char *) "Of:find_range", (char **)keywords,
	                                 &py_co, &radius))
	{
		return NULL;
	}

	if (mathutils_array_parse(co, 3, 3, py_co, "find_range: invalid 'co' arg") == -1)
		return NULL;

	if (radius < 0.0f) {
		PyErr_SetString(PyExc_RuntimeError, "negative radius given");
		return NULL;
	}

	if (self->count != self->count_balance) {
		PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_range()");
		return NULL;
	}

	found = BLI_kdtree_range_search(self->obj, co, NULL, &nearest, radius);

	py_list = PyList_New(found);

	for (i = 0; i < found; i++) {
		PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py(&nearest[i]));
	}

	if (nearest) {
		MEM_freeN(nearest);
	}

	return py_list;
}


static PyMethodDef PyKDTree_methods[] = {
	{"insert", (PyCFunction)py_kdtree_insert, METH_VARARGS | METH_KEYWORDS, py_kdtree_insert_doc},
	{"balance", (PyCFunction)py_kdtree_balance, METH_NOARGS, py_kdtree_balance_doc},
	{"find", (PyCFunction)py_kdtree_find, METH_VARARGS | METH_KEYWORDS, py_kdtree_find_doc},
	{"find_n", (PyCFunction)py_kdtree_find_n, METH_VARARGS | METH_KEYWORDS, py_kdtree_find_n_doc},
	{"find_range", (PyCFunction)py_kdtree_find_range, METH_VARARGS | METH_KEYWORDS, py_kdtree_find_range_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(py_KDtree_doc,
"KdTree(size) -> new kd-tree initialized to hold ``size`` items.\n"
"\n"
".. note::\n"
"\n"
"   :class:`KDTree.balance` must have been called before using any of the ``find`` methods.\n"
);
PyTypeObject PyKDTree_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KDTree",                                    /* tp_name */
	sizeof(PyKDTree),                            /* tp_basicsize */
	0,                                           /* tp_itemsize */
	/* methods */
	(destructor)PyKDTree__tp_dealloc,            /* tp_dealloc */
	NULL,                                        /* tp_print */
	NULL,                                        /* tp_getattr */
	NULL,                                        /* tp_setattr */
	NULL,                                        /* tp_compare */
	NULL,                                        /* tp_repr */
	NULL,                                        /* tp_as_number */
	NULL,                                        /* tp_as_sequence */
	NULL,                                        /* tp_as_mapping */
	NULL,                                        /* tp_hash */
	NULL,                                        /* tp_call */
	NULL,                                        /* tp_str */
	NULL,                                        /* tp_getattro */
	NULL,                                        /* tp_setattro */
	NULL,                                        /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                          /* tp_flags */
	py_KDtree_doc,                               /* Documentation string */
	NULL,                                        /* tp_traverse */
	NULL,                                        /* tp_clear */
	NULL,                                        /* tp_richcompare */
	0,                                           /* tp_weaklistoffset */
	NULL,                                        /* tp_iter */
	NULL,                                        /* tp_iternext */
	(struct PyMethodDef *)PyKDTree_methods,      /* tp_methods */
	NULL,                                        /* tp_members */
	NULL,                                        /* tp_getset */
	NULL,                                        /* tp_base */
	NULL,                                        /* tp_dict */
	NULL,                                        /* tp_descr_get */
	NULL,                                        /* tp_descr_set */
	0,                                           /* tp_dictoffset */
	(initproc)PyKDTree__tp_init,                 /* tp_init */
	(allocfunc)PyType_GenericAlloc,              /* tp_alloc */
	(newfunc)PyType_GenericNew,                  /* tp_new */
	(freefunc)0,                                 /* tp_free */
	NULL,                                        /* tp_is_gc */
	NULL,                                        /* tp_bases */
	NULL,                                        /* tp_mro */
	NULL,                                        /* tp_cache */
	NULL,                                        /* tp_subclasses */
	NULL,                                        /* tp_weaklist */
	(destructor) NULL                            /* tp_del */
};

PyDoc_STRVAR(py_kdtree_doc,
"Generic 3-dimentional kd-tree to perform spatial searches."
);
static struct PyModuleDef kdtree_moduledef = {
	PyModuleDef_HEAD_INIT,
	"mathutils.kdtree",                          /* m_name */
	py_kdtree_doc,                               /* m_doc */
	0,                                           /* m_size */
	NULL,                                        /* m_methods */
	NULL,                                        /* m_reload */
	NULL,                                        /* m_traverse */
	NULL,                                        /* m_clear */
	NULL                                         /* m_free */
};

PyMODINIT_FUNC PyInit_mathutils_kdtree(void)
{
	PyObject *m = PyModule_Create(&kdtree_moduledef);

	if (m == NULL) {
		return NULL;
	}

	/* Register the 'KDTree' class */
	if (PyType_Ready(&PyKDTree_Type)) {
		return NULL;
	}
	PyModule_AddObject(m, (char *)"KDTree", (PyObject *) &PyKDTree_Type);

	return m;
}
