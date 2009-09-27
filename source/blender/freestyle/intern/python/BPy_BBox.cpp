#include "BPy_BBox.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for BBox instance  -----------*/
static int BBox___init__(BPy_BBox *self, PyObject *args, PyObject *kwds);
static void BBox___dealloc__(BPy_BBox *self);
static PyObject * BBox___repr__(BPy_BBox *self);

/*----------------------BBox instance definitions ----------------------------*/
static PyMethodDef BPy_BBox_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_BBox type definition ------------------------------*/

PyTypeObject BBox_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BBox",                         /* tp_name */
	sizeof(BPy_BBox),               /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)BBox___dealloc__,   /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)BBox___repr__,        /* tp_repr */
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
	"BBox objects",                 /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_BBox_methods,               /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)BBox___init__,        /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int BBox_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &BBox_Type ) < 0 )
		return -1;

	Py_INCREF( &BBox_Type );
	PyModule_AddObject(module, "BBox", (PyObject *)&BBox_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int BBox___init__(BPy_BBox *self, PyObject *args, PyObject *kwds)
{
	if(!( PyArg_ParseTuple(args, "") ))
		return -1;
	self->bb = new BBox< Vec3r>();
	return 0;
}

void BBox___dealloc__(BPy_BBox* self)
{
	delete self->bb;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * BBox___repr__(BPy_BBox* self)
{
    return PyUnicode_FromFormat("BBox - address: %p", self->bb );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
