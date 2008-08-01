#include "BPy_Interface1D.h"

#include "BPy_Convert.h"
#include "Interface1D/BPy_FrsCurve.h"
#include "Interface1D/Curve/BPy_Chain.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/FEdge/BPy_FEdgeSharp.h"
#include "Interface1D/FEdge/BPy_FEdgeSmooth.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"

#include "BPy_MediumType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Interface1D instance  -----------*/
static int Interface1D___init__(BPy_Interface1D *self, PyObject *args, PyObject *kwds);
static void Interface1D___dealloc__(BPy_Interface1D *self);
static PyObject * Interface1D___repr__(BPy_Interface1D *self);

static PyObject *Interface1D_getExactTypeName( BPy_Interface1D *self );
static PyObject *Interface1D_getVertices( BPy_Interface1D *self );
static PyObject *Interface1D_getPoints( BPy_Interface1D *self );
static PyObject *Interface1D_getLength2D( BPy_Interface1D *self );
static PyObject *Interface1D_getId( BPy_Interface1D *self );
static PyObject *Interface1D_getNature( BPy_Interface1D *self );
static PyObject *Interface1D_getTimeStamp( BPy_Interface1D *self );
static PyObject *Interface1D_setTimeStamp( BPy_Interface1D *self, PyObject *args);

/*----------------------Interface1D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface1D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface1D_getExactTypeName, METH_NOARGS, "（ ）Returns the string of the name of the interface."},
	{"getVertices", ( PyCFunction ) Interface1D_getVertices, METH_NOARGS, "Returns the vertices"},
	{"getPoints", ( PyCFunction ) Interface1D_getPoints, METH_NOARGS, "Returns the points. The difference with getVertices() is that here we can iterate over points of the 1D element at any given sampling. At each call, a virtual point is created."},
	{"getLength2D", ( PyCFunction ) Interface1D_getLength2D, METH_NOARGS, "Returns the 2D length of the 1D element"},
	{"getId", ( PyCFunction ) Interface1D_getId, METH_NOARGS, "Returns the Id of the 1D element"},
	{"getNature", ( PyCFunction ) Interface1D_getNature, METH_NOARGS, "Returns the nature of the 1D element"},
	{"getTimeStamp", ( PyCFunction ) Interface1D_getTimeStamp, METH_NOARGS, "Returns the time stamp of the 1D element. Mainly used for selection"},
	{"setTimeStamp", ( PyCFunction ) Interface1D_setTimeStamp, METH_VARARGS, "Sets the time stamp for the 1D element"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface1D type definition ------------------------------*/

PyTypeObject Interface1D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Interface1D",				/* tp_name */
	sizeof( BPy_Interface1D ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Interface1D___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)Interface1D___repr__,					/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Interface1D_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Interface1D___init__,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC Interface1D_Init( PyObject *module )
{
	PyObject *tmp;
	
	if( module == NULL )
		return;

	if( PyType_Ready( &Interface1D_Type ) < 0 )
		return;
	Py_INCREF( &Interface1D_Type );
	PyModule_AddObject(module, "Interface1D", (PyObject *)&Interface1D_Type);
	
	if( PyType_Ready( &FrsCurve_Type ) < 0 )
		return;
	Py_INCREF( &FrsCurve_Type );
	PyModule_AddObject(module, "FrsCurve", (PyObject *)&FrsCurve_Type);

	if( PyType_Ready( &Chain_Type ) < 0 )
		return;
	Py_INCREF( &Chain_Type );
	PyModule_AddObject(module, "Chain", (PyObject *)&Chain_Type);
	
	if( PyType_Ready( &FEdge_Type ) < 0 )
		return;
	Py_INCREF( &FEdge_Type );
	PyModule_AddObject(module, "FEdge", (PyObject *)&FEdge_Type);

	if( PyType_Ready( &FEdgeSharp_Type ) < 0 )
		return;
	Py_INCREF( &FEdgeSharp_Type );
	PyModule_AddObject(module, "FEdgeSharp", (PyObject *)&FEdgeSharp_Type);
	
	if( PyType_Ready( &FEdgeSmooth_Type ) < 0 )
		return;
	Py_INCREF( &FEdgeSmooth_Type );
	PyModule_AddObject(module, "FEdgeSmooth", (PyObject *)&FEdgeSmooth_Type);

	if( PyType_Ready( &Stroke_Type ) < 0 )
		return;
	Py_INCREF( &Stroke_Type );
	PyModule_AddObject(module, "Stroke", (PyObject *)&Stroke_Type);

	tmp = BPy_MediumType_from_MediumType( Stroke::DRY_MEDIUM ); PyDict_SetItemString( Stroke_Type.tp_dict, "DRY_MEDIUM", tmp); Py_DECREF(tmp);
	tmp = BPy_MediumType_from_MediumType( Stroke::HUMID_MEDIUM ); PyDict_SetItemString( Stroke_Type.tp_dict, "HUMID_MEDIUM", tmp); Py_DECREF(tmp);
	tmp = BPy_MediumType_from_MediumType( Stroke::OPAQUE_MEDIUM ); PyDict_SetItemString( Stroke_Type.tp_dict, "OPAQUE_MEDIUM", tmp); Py_DECREF(tmp);

	if( PyType_Ready( &ViewEdge_Type ) < 0 )
		return;
	Py_INCREF( &ViewEdge_Type );
	PyModule_AddObject(module, "ViewEdge", (PyObject *)&ViewEdge_Type);

	
}

//------------------------INSTANCE METHODS ----------------------------------

int Interface1D___init__(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
	self->if1D = new Interface1D();
	return 0;
}

void Interface1D___dealloc__(BPy_Interface1D* self)
{
	delete self->if1D;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * Interface1D___repr__(BPy_Interface1D* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->if1D->getExactTypeName().c_str(), self->if1D );
}

PyObject *Interface1D_getExactTypeName( BPy_Interface1D *self ) {
	return PyString_FromString( self->if1D->getExactTypeName().c_str() );	
}

PyObject *Interface1D_getVertices( BPy_Interface1D *self ) {
	return PyList_New(0);
}

PyObject *Interface1D_getPoints( BPy_Interface1D *self ) {
	return PyList_New(0);
}

PyObject *Interface1D_getLength2D( BPy_Interface1D *self ) {
	return PyFloat_FromDouble( (double) self->if1D->getLength2D() );
}

PyObject *Interface1D_getId( BPy_Interface1D *self ) {
	Id id( self->if1D->getId() );
	return BPy_Id_from_Id( id );
}

PyObject *Interface1D_getNature( BPy_Interface1D *self ) {
	return BPy_Nature_from_Nature( self->if1D->getNature() );
}

PyObject *Interface1D_getTimeStamp( BPy_Interface1D *self ) {
	return PyInt_FromLong( self->if1D->getTimeStamp() );
}

PyObject *Interface1D_setTimeStamp( BPy_Interface1D *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"i", &timestamp) ) {
		cout << "ERROR: Interface1D_setTimeStamp" << endl;
		Py_RETURN_NONE;
	}
	
	self->if1D->setTimeStamp( timestamp );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


