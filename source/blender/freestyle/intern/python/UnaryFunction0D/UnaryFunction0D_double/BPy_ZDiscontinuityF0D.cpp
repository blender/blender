#include "BPy_ZDiscontinuityF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ZDiscontinuityF0D___doc__[] =
".. method:: __init__()\n"
"\n"
"   Builds a ZDiscontinuityF0D object.\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns a real value giving the distance between the\n"
"   :class:`Interface0D` pointed by the Interface0DIterator and the\n"
"   shape that lies behind (occludee).  This distance is evaluated in\n"
"   the camera space and normalized between 0 and 1.  Therefore, if no\n"
"   oject is occluded by the shape to which the Interface0D belongs to,\n"
"   1 is returned.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`\n"
"   :return: The normalized distance between the pointed Interface0D\n"
"      and the occludee.\n"
"   :rtype: float\n";

static int ZDiscontinuityF0D___init__( BPy_ZDiscontinuityF0D* self, PyObject *args )
{
	if( !PyArg_ParseTuple(args, "") )
		return -1;
	self->py_uf0D_double.uf0D_double = new Functions0D::ZDiscontinuityF0D();
	self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_ZDiscontinuityF0D type definition ------------------------------*/

PyTypeObject ZDiscontinuityF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ZDiscontinuityF0D",            /* tp_name */
	sizeof(BPy_ZDiscontinuityF0D),  /* tp_basicsize */
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
	ZDiscontinuityF0D___doc__,      /* tp_doc */
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
	(initproc)ZDiscontinuityF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
