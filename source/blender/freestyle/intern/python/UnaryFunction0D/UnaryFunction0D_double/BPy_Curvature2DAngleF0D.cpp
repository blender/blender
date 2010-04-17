#include "BPy_Curvature2DAngleF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char Curvature2DAngleF0D___doc__[] =
".. method:: __init__()\n"
"\n"
"   Builds a Curvature2DAngleF0D object.\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns a real value giving the 2D curvature (as an angle) of the\n"
"   1D element to which the :class:`Interface0D` pointed by the\n"
"   Interface0DIterator belongs.  The 2D curvature is evaluated at the\n"
"   Interface0D.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`\n"
"   :return: The 2D curvature of the 1D element evaluated at the\n"
"      pointed Interface0D.\n"
"   :rtype: float\n";

static int Curvature2DAngleF0D___init__( BPy_Curvature2DAngleF0D* self, PyObject *args )
{
	if( !PyArg_ParseTuple(args, "") )
		return -1;
	self->py_uf0D_double.uf0D_double = new Functions0D::Curvature2DAngleF0D();
	self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_Curvature2DAngleF0D type definition ------------------------------*/

PyTypeObject Curvature2DAngleF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Curvature2DAngleF0D",          /* tp_name */
	sizeof(BPy_Curvature2DAngleF0D), /* tp_basicsize */
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
	Curvature2DAngleF0D___doc__,    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DDouble_Type,    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Curvature2DAngleF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
