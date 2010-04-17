#include "BPy_GetProjectedYF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GetProjectedYF0D___doc__[] =
".. method:: __init__()\n"
"\n"
"   Builds a GetProjectedYF0D object.\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns the Y 3D projected coordinate of the :class:`Interface0D`\n"
"   pointed by the Interface0DIterator.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`\n"
"   :return: The Y 3D projected coordinate of the pointed Interface0D.\n"
"   :rtype: float\n";

static int GetProjectedYF0D___init__( BPy_GetProjectedYF0D* self, PyObject *args )
{
	if( !PyArg_ParseTuple(args, "") )
		return -1;
	self->py_uf0D_double.uf0D_double = new Functions0D::GetProjectedYF0D();
	self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_GetProjectedYF0D type definition ------------------------------*/

PyTypeObject GetProjectedYF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GetProjectedYF0D",             /* tp_name */
	sizeof(BPy_GetProjectedYF0D),   /* tp_basicsize */
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
	GetProjectedYF0D___doc__,       /* tp_doc */
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
	(initproc)GetProjectedYF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
