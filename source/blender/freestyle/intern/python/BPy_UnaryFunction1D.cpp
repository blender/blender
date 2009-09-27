#include "BPy_UnaryFunction1D.h"

#include "UnaryFunction1D/BPy_UnaryFunction1DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DEdgeNature.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DFloat.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DUnsigned.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec2f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec3f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVectorViewShape.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction1D instance  -----------*/

static void UnaryFunction1D___dealloc__(BPy_UnaryFunction1D *self);
static PyObject * UnaryFunction1D___repr__(BPy_UnaryFunction1D *self);

/*-----------------------BPy_UnaryFunction1D type definition ------------------------------*/

PyTypeObject UnaryFunction1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction1D",              /* tp_name */
	sizeof(BPy_UnaryFunction1D),    /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction1D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction1D___repr__, /* tp_repr */
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
	"UnaryFunction1D objects",      /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryFunction1D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction1D_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction1D_Type );
	PyModule_AddObject(module, "UnaryFunction1D", (PyObject *)&UnaryFunction1D_Type);

	UnaryFunction1DDouble_Init( module );
	UnaryFunction1DEdgeNature_Init( module );
	UnaryFunction1DFloat_Init( module );
	UnaryFunction1DUnsigned_Init( module );
	UnaryFunction1DVec2f_Init( module );
	UnaryFunction1DVec3f_Init( module );
	UnaryFunction1DVectorViewShape_Init( module );
	UnaryFunction1DVoid_Init( module );
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

void UnaryFunction1D___dealloc__(BPy_UnaryFunction1D* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * UnaryFunction1D___repr__(BPy_UnaryFunction1D* self)
{
    return PyUnicode_FromFormat("UnaryFunction1D");
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
