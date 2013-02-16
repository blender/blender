#include "BPy_CurvePointIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(CurvePointIterator_doc,
"Class hierarchy: :class:`Iterator` > :class:`CurvePointIterator`\n"
"\n"
"Class representing an iterator on a curve.  Allows an iterating\n"
"outside initial vertices.  A CurvePoint is instanciated and returned\n"
"by getObject().\n"
"\n"
".. method:: __init__(step=0.0)\n"
"\n"
"   Builds a CurvePointIterator object.\n"
"\n"
"   :arg step: A resampling resolution with which the curve is resampled.\n"
"      If zero, no resampling is done (i.e., the iterator iterates over\n"
"      initial vertices).\n"
"   :type step: float\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A CurvePointIterator object.\n"
"   :type brother: :class:`CurvePointIterator`");

static int CurvePointIterator_init(BPy_CurvePointIterator *self, PyObject *args)
{
	PyObject *obj = 0;

	if (!PyArg_ParseTuple(args, "|O", &obj))
		return -1;

	if (!obj) {
		self->cp_it = new CurveInternal::CurvePointIterator();

	} else if (BPy_CurvePointIterator_Check(obj)) {
		self->cp_it = new CurveInternal::CurvePointIterator(*(((BPy_CurvePointIterator *)obj)->cp_it));

	} else if (PyFloat_Check(obj)) {
		self->cp_it = new CurveInternal::CurvePointIterator(PyFloat_AsDouble(obj));

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_it.it = self->cp_it;

	return 0;
}

PyDoc_STRVAR(CurvePointIterator_cast_to_interface0diterator_doc,
".. method:: cast_to_interface0diterator()\n"
"\n"
"   Returns an Interface0DIterator converted from this\n"
"   CurvePointIterator.  Useful for any call to a function of the\n"
"   UnaryFunction0D type.\n"
"\n"
"   :return: An Interface0DIterator object converted from the\n"
"      iterator.\n"
"   :rtype: :class:`Interface0DIterator`");

static PyObject * CurvePointIterator_cast_to_interface0diterator(BPy_CurvePointIterator *self)
{
	Interface0DIterator it(self->cp_it->castToInterface0DIterator());
	return BPy_Interface0DIterator_from_Interface0DIterator(it, 0);
}

static PyMethodDef BPy_CurvePointIterator_methods[] = {
	{"cast_to_interface0diterator", (PyCFunction) CurvePointIterator_cast_to_interface0diterator, METH_NOARGS, CurvePointIterator_cast_to_interface0diterator_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------CurvePointIterator get/setters ----------------------------*/

PyDoc_STRVAR(CurvePointIterator_object_doc,
"The CurvePoint object currently pointed by this iterator.\n"
"\n"
":type: :class:`CurvePoint`");

static PyObject *CurvePointIterator_object_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
{
	return BPy_CurvePoint_from_CurvePoint(self->cp_it->operator*());
}

PyDoc_STRVAR(CurvePointIterator_t_doc,
"The curvilinear abscissa of the current point.\n"
"\n"
":type: float");

static PyObject *CurvePointIterator_t_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->cp_it->t());
}

PyDoc_STRVAR(CurvePointIterator_u_doc,
"The point parameter at the current point in the stroke (0 <= u <= 1).\n"
"\n"
":type: float");

static PyObject *CurvePointIterator_u_get(BPy_CurvePointIterator *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->cp_it->u());
}

static PyGetSetDef BPy_CurvePointIterator_getseters[] = {
	{(char *)"object", (getter)CurvePointIterator_object_get, (setter)NULL, (char *)CurvePointIterator_object_doc, NULL},
	{(char *)"t", (getter)CurvePointIterator_t_get, (setter)NULL, (char *)CurvePointIterator_t_doc, NULL},
	{(char *)"u", (getter)CurvePointIterator_u_get, (setter)NULL, (char *)CurvePointIterator_u_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_CurvePointIterator type definition ------------------------------*/

PyTypeObject CurvePointIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"CurvePointIterator",           /* tp_name */
	sizeof(BPy_CurvePointIterator), /* tp_basicsize */
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
	CurvePointIterator_doc,         /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_CurvePointIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	BPy_CurvePointIterator_getseters, /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CurvePointIterator_init, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
