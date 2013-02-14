#include "BPy_NonTVertex.h"

#include "../../BPy_Convert.h"
#include "../BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------NonTVertex methods ----------------------------*/

PyDoc_STRVAR(NonTVertex_doc,
"Class hierarchy: :class:`Interface0D` > :class:`ViewVertex` > :class:`NonTVertex`\n"
"\n"
"View vertex for corners, cusps, etc. associated to a single SVertex.\n"
"Can be associated to 2 or more view edges.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A NonTVertex object.\n"
"   :type iBrother: :class:`NonTVertex`\n"
"\n"
".. method:: __init__(iSVertex)\n"
"\n"
"   Builds a NonTVertex from a SVertex.\n"
"\n"
"   :arg iSVertex: An SVertex object.\n"
"   :type iSVertex: :class:`SVertex`");

static int NonTVertex_init(BPy_NonTVertex *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj = 0;

	if (!PyArg_ParseTuple(args, "|O!", &SVertex_Type, &obj))
		return -1;

	if (!obj) {
		self->ntv = new NonTVertex();

	} else if(((BPy_SVertex *)obj)->sv) {
		self->ntv = new NonTVertex(((BPy_SVertex *)obj)->sv);

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_vv.vv = self->ntv;
	self->py_vv.py_if0D.if0D = self->ntv;
	self->py_vv.py_if0D.borrowed = 0;

	return 0;
}

static PyMethodDef BPy_NonTVertex_methods[] = {	
	{NULL, NULL, 0, NULL}
};

/*----------------------NonTVertex get/setters ----------------------------*/

PyDoc_STRVAR(NonTVertex_svertex_doc,
"The SVertex on top of which this NonTVertex is built.\n"
"\n"
":type: :class:`SVertex`");

static PyObject *NonTVertex_svertex_get(BPy_NonTVertex *self, void *UNUSED(closure))
{
	SVertex *v = self->ntv->svertex();
	if (v)
		return BPy_SVertex_from_SVertex(*v);
	Py_RETURN_NONE;
}

static int NonTVertex_svertex_set(BPy_NonTVertex *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_SVertex_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
		return -1;
	}
	self->ntv->setSVertex(((BPy_SVertex *)value)->sv);
	return 0;
}

static PyGetSetDef BPy_NonTVertex_getseters[] = {
	{(char *)"svertex", (getter)NonTVertex_svertex_get, (setter)NonTVertex_svertex_set, (char *)NonTVertex_svertex_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_NonTVertex type definition ------------------------------*/
PyTypeObject NonTVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"NonTVertex",                   /* tp_name */
	sizeof(BPy_NonTVertex),         /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	NonTVertex_doc,                 /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_NonTVertex_methods,         /* tp_methods */
	0,                              /* tp_members */
	BPy_NonTVertex_getseters,       /* tp_getset */
	&ViewVertex_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)NonTVertex_init,      /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
