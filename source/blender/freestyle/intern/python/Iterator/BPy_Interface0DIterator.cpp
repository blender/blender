#include "BPy_Interface0DIterator.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(Interface0DIterator_doc,
"Class hierarchy: :class:`Iterator` > :class:`Interface0DIterator`\n"
"\n"
"Class defining an iterator over Interface0D elements.  An instance of\n"
"this iterator is always obtained from a 1D element.\n"
"\n"
".. method:: __init__(it)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`");

static int Interface0DIterator_init(BPy_Interface0DIterator *self, PyObject *args)
{
	PyObject *obj = 0;

	if (!PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj))
		return -1;

	self->if0D_it = new Interface0DIterator(*(((BPy_Interface0DIterator *)obj)->if0D_it));
	self->py_it.it = self->if0D_it;
	self->reversed = 0;

	return 0;
}

static PyObject * Interface0DIterator_iternext(BPy_Interface0DIterator *self)
{
	Interface0D *if0D;

	if (self->reversed) {
		if (self->if0D_it->isBegin()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		self->if0D_it->decrement();
		if0D = self->if0D_it->operator->();
	} else {
		if (self->if0D_it->isEnd()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		if0D = self->if0D_it->operator->();
		self->if0D_it->increment();
	}
	return Any_BPy_Interface0D_from_Interface0D(*if0D);
}

static PyMethodDef BPy_Interface0DIterator_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*----------------------Interface0DIterator get/setters ----------------------------*/

PyDoc_STRVAR(Interface0DIterator_object_doc,
"The Interface0D object currently pointed by this iterator.\n"
"\n"
":type: :class:`Interface0D`");

static PyObject *Interface0DIterator_object_get(BPy_Interface0DIterator *self, void *UNUSED(closure))
{
	return Any_BPy_Interface0D_from_Interface0D(self->if0D_it->operator*());
}

PyDoc_STRVAR(Interface0DIterator_t_doc,
"The curvilinear abscissa of the current point.\n"
"\n"
":type: float");

static PyObject *Interface0DIterator_t_get(BPy_Interface0DIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->if0D_it->t());
}

PyDoc_STRVAR(Interface0DIterator_u_doc,
"The point parameter at the current point in the 1D element (0 <= u <= 1).\n"
"\n"
":type: float");

static PyObject *Interface0DIterator_u_get(BPy_Interface0DIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->if0D_it->u());
}

static PyGetSetDef BPy_Interface0DIterator_getseters[] = {
	{(char *)"object", (getter)Interface0DIterator_object_get, (setter)NULL, (char *)Interface0DIterator_object_doc, NULL},
	{(char *)"t", (getter)Interface0DIterator_t_get, (setter)NULL, (char *)Interface0DIterator_t_doc, NULL},
	{(char *)"u", (getter)Interface0DIterator_u_get, (setter)NULL, (char *)Interface0DIterator_u_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_Interface0DIterator type definition ------------------------------*/

PyTypeObject Interface0DIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Interface0DIterator",          /* tp_name */
	sizeof(BPy_Interface0DIterator), /* tp_basicsize */
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
	Interface0DIterator_doc,        /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)Interface0DIterator_iternext, /* tp_iternext */
	BPy_Interface0DIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	BPy_Interface0DIterator_getseters, /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Interface0DIterator_init, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
