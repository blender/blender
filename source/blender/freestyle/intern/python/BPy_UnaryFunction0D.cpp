#include "BPy_UnaryFunction0D.h"

#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DEdgeNature.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DId.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DMaterial.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec2f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec3f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVectorViewShape.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DViewShape.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0D instance  -----------*/

static void UnaryFunction0D___dealloc__(BPy_UnaryFunction0D *self);
static PyObject * UnaryFunction0D___repr__(BPy_UnaryFunction0D *self);

/*-----------------------BPy_UnaryFunction0D type definition ------------------------------*/

PyTypeObject UnaryFunction0D_Type = {
	PyObject_HEAD_INIT(NULL)
	"UnaryFunction0D",              /* tp_name */
	sizeof(BPy_UnaryFunction0D),    /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction0D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction0D___repr__, /* tp_repr */
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
	"UnaryFunction0D objects",      /* tp_doc */
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
int UnaryFunction0D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction0D_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction0D_Type );
	PyModule_AddObject(module, "UnaryFunction0D", (PyObject *)&UnaryFunction0D_Type);

	UnaryFunction0DDouble_Init( module );
	UnaryFunction0DEdgeNature_Init( module );
	UnaryFunction0DFloat_Init( module );
	UnaryFunction0DId_Init( module );
	UnaryFunction0DMaterial_Init( module );
	UnaryFunction0DUnsigned_Init( module );
	UnaryFunction0DVec2f_Init( module );
	UnaryFunction0DVec3f_Init( module );
	UnaryFunction0DVectorViewShape_Init( module );
	UnaryFunction0DViewShape_Init( module );
	
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

void UnaryFunction0D___dealloc__(BPy_UnaryFunction0D* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * UnaryFunction0D___repr__(BPy_UnaryFunction0D* self)
{
    return PyUnicode_FromFormat("UnaryFunction0D");
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
