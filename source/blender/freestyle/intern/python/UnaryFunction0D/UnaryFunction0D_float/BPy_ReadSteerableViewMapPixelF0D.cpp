#include "BPy_ReadSteerableViewMapPixelF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ReadSteerableViewMapPixelF0D instance  -----------*/
static int ReadSteerableViewMapPixelF0D___init__(BPy_ReadSteerableViewMapPixelF0D* self, PyObject *args);

/*-----------------------BPy_ReadSteerableViewMapPixelF0D type definition ------------------------------*/

PyTypeObject ReadSteerableViewMapPixelF0D_Type = {
	PyObject_HEAD_INIT(NULL)
	"ReadSteerableViewMapPixelF0D",    /* tp_name */
	sizeof(BPy_ReadSteerableViewMapPixelF0D), /* tp_basicsize */
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
	"ReadSteerableViewMapPixelF0D objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DFloat_Type,     /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ReadSteerableViewMapPixelF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ReadSteerableViewMapPixelF0D___init__( BPy_ReadSteerableViewMapPixelF0D* self, PyObject *args)
{
	unsigned int u;
	int i;

	if( !PyArg_ParseTuple(args, "Ii", &u, &i) )
		return -1;
	self->py_uf0D_float.uf0D_float = new Functions0D::ReadSteerableViewMapPixelF0D(u,i);
	self->py_uf0D_float.uf0D_float->py_uf0D = (PyObject *)self;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
