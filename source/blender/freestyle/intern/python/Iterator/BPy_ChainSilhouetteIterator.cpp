#include "BPy_ChainSilhouetteIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ChainSilhouetteIterator instance  -----------*/
static int ChainSilhouetteIterator___init__(BPy_ChainSilhouetteIterator *self, PyObject *args);

/*-----------------------BPy_ChainSilhouetteIterator type definition ------------------------------*/

PyTypeObject ChainSilhouetteIterator_Type = {
	PyObject_HEAD_INIT(NULL)
	"ChainSilhouetteIterator",      /* tp_name */
	sizeof(BPy_ChainSilhouetteIterator), /* tp_basicsize */
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
	"ChainSilhouetteIterator objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ChainingIterator_Type,         /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ChainSilhouetteIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

// ChainSilhouetteIterator (bool iRestrictToSelection=true, ViewEdge *begin=NULL, bool orientation=true)
// ChainSilhouetteIterator (const ChainSilhouetteIterator &brother)

int ChainSilhouetteIterator___init__(BPy_ChainSilhouetteIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

	if (!( PyArg_ParseTuple(args, "|OOO", &obj1, &obj2, &obj3) ))
	    return -1;

	if( obj1 && BPy_ChainSilhouetteIterator_Check(obj1)  ) { 
		self->cs_it = new ChainSilhouetteIterator(*( ((BPy_ChainSilhouetteIterator *) obj1)->cs_it ));
	
	} else {
		bool restrictToSelection = ( obj1 ) ? bool_from_PyBool(obj1) : true;
		ViewEdge *begin;
		if ( !obj2 || obj2 == Py_None )
			begin = NULL;
		else if ( BPy_ViewEdge_Check(obj2) )
			begin = ((BPy_ViewEdge *) obj2)->ve;
		else {
			PyErr_SetString(PyExc_TypeError, "2nd argument must be either a ViewEdge object or None");
			return -1;
		}
		bool orientation = ( obj3 ) ? bool_from_PyBool(obj3) : true;
		
		self->cs_it = new ChainSilhouetteIterator( restrictToSelection, begin, orientation);	
	}
	
	self->py_c_it.c_it = self->cs_it;
	self->py_c_it.py_ve_it.ve_it = self->cs_it;
	self->py_c_it.py_ve_it.py_it.it = self->cs_it;
	
	return 0;
	
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
