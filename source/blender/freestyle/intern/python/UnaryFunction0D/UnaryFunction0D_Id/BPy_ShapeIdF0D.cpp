#include "BPy_ShapeIdF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ShapeIdF0D instance  -----------*/
static int ShapeIdF0D___init__(BPy_ShapeIdF0D* self, PyObject *args);

/*-----------------------BPy_ShapeIdF0D type definition ------------------------------*/

PyTypeObject ShapeIdF0D_Type = {
	PyObject_HEAD_INIT(NULL)
	"ShapeIdF0D",                   /* tp_name */
	sizeof(BPy_ShapeIdF0D),         /* tp_basicsize */
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
	"ShapeIdF0D objects",           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DId_Type,        /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ShapeIdF0D___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ShapeIdF0D___init__( BPy_ShapeIdF0D* self, PyObject *args )
{
	if( !PyArg_ParseTuple(args, "") )
		return -1;
	self->py_uf0D_id.uf0D_id = new Functions0D::ShapeIdF0D();
	self->py_uf0D_id.uf0D_id->py_uf0D = (PyObject *)self;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
