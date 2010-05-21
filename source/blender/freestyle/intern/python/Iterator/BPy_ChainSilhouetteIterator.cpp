#include "BPy_ChainSilhouetteIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

// ChainSilhouetteIterator (bool iRestrictToSelection=true, ViewEdge *begin=NULL, bool orientation=true)
// ChainSilhouetteIterator (const ChainSilhouetteIterator &brother)

static char ChainSilhouetteIterator___doc__[] =
"A ViewEdge Iterator used to follow ViewEdges the most naturally.  For\n"
"example, it will follow visible ViewEdges of same nature.  As soon, as\n"
"the nature or the visibility changes, the iteration stops (by setting\n"
"the pointed ViewEdge to 0).  In the case of an iteration over a set of\n"
"ViewEdge that are both Silhouette and Crease, there will be a\n"
"precedence of the silhouette over the crease criterion.\n"
"\n"
".. method:: __init__(iRestrictToSelection=True, begin=None, orientation=True)\n"
"\n"
"   Builds a ChainSilhouetteIterator from the first ViewEdge used for\n"
"   iteration and its orientation.\n"
"\n"
"   :arg iRestrictToSelection: Indicates whether to force the chaining\n"
"      to stay within the set of selected ViewEdges or not.\n"
"   :type iRestrictToSelection: bool\n"
"   :arg begin: The ViewEdge from where to start the iteration.\n"
"   :type begin: :class:`ViewEdge` or None\n"
"   :arg orientation: If true, we'll look for the next ViewEdge among\n"
"      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
"      false, we'll search over the ViewEdges surrounding the ending\n"
"      ViewVertex of begin.\n"
"   :type orientation: bool\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A ChainSilhouetteIterator object.\n"
"   :type brother: :class:`ChainSilhouetteIterator`\n";

static int ChainSilhouetteIterator___init__(BPy_ChainSilhouetteIterator *self, PyObject *args )
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

/*-----------------------BPy_ChainSilhouetteIterator type definition ------------------------------*/

PyTypeObject ChainSilhouetteIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	ChainSilhouetteIterator___doc__, /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
