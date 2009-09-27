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
static PyObject * Interface1D_verticesBegin( BPy_Interface1D *self );
static PyObject * Interface1D_verticesEnd( BPy_Interface1D *self );
static PyObject * Interface1D_pointsBegin( BPy_Interface1D *self, PyObject *args );
static PyObject * Interface1D_pointsEnd( BPy_Interface1D *self, PyObject *args );

/*----------------------Interface1D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface1D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface1D_getExactTypeName, METH_NOARGS, "() Returns the string of the name of the interface."},
	{"getVertices", ( PyCFunction ) Interface1D_getVertices, METH_NOARGS, "Returns the vertices"},
	{"getPoints", ( PyCFunction ) Interface1D_getPoints, METH_NOARGS, "Returns the points. The difference with getVertices() is that here we can iterate over points of the 1D element at any given sampling. At each call, a virtual point is created."},
	{"getLength2D", ( PyCFunction ) Interface1D_getLength2D, METH_NOARGS, "Returns the 2D length of the 1D element"},
	{"getId", ( PyCFunction ) Interface1D_getId, METH_NOARGS, "Returns the Id of the 1D element"},
	{"getNature", ( PyCFunction ) Interface1D_getNature, METH_NOARGS, "Returns the nature of the 1D element"},
	{"getTimeStamp", ( PyCFunction ) Interface1D_getTimeStamp, METH_NOARGS, "Returns the time stamp of the 1D element. Mainly used for selection"},
	{"setTimeStamp", ( PyCFunction ) Interface1D_setTimeStamp, METH_VARARGS, "Sets the time stamp for the 1D element"},
	{"verticesBegin", ( PyCFunction ) Interface1D_verticesBegin, METH_NOARGS, ""},
	{"verticesEnd", ( PyCFunction ) Interface1D_verticesEnd, METH_NOARGS, ""},
	{"pointsBegin", ( PyCFunction ) Interface1D_pointsBegin, METH_VARARGS, ""},
	{"pointsEnd", ( PyCFunction ) Interface1D_pointsEnd, METH_VARARGS, ""},

	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface1D type definition ------------------------------*/

PyTypeObject Interface1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Interface1D",                  /* tp_name */
	sizeof(BPy_Interface1D),        /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Interface1D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)Interface1D___repr__, /* tp_repr */
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
	"Interface1D objects",          /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Interface1D_methods,        /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Interface1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int Interface1D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &Interface1D_Type ) < 0 )
		return -1;
	Py_INCREF( &Interface1D_Type );
	PyModule_AddObject(module, "Interface1D", (PyObject *)&Interface1D_Type);
	
	if( PyType_Ready( &FrsCurve_Type ) < 0 )
		return -1;
	Py_INCREF( &FrsCurve_Type );
	PyModule_AddObject(module, "FrsCurve", (PyObject *)&FrsCurve_Type);

	if( PyType_Ready( &Chain_Type ) < 0 )
		return -1;
	Py_INCREF( &Chain_Type );
	PyModule_AddObject(module, "Chain", (PyObject *)&Chain_Type);
	
	if( PyType_Ready( &FEdge_Type ) < 0 )
		return -1;
	Py_INCREF( &FEdge_Type );
	PyModule_AddObject(module, "FEdge", (PyObject *)&FEdge_Type);

	if( PyType_Ready( &FEdgeSharp_Type ) < 0 )
		return -1;
	Py_INCREF( &FEdgeSharp_Type );
	PyModule_AddObject(module, "FEdgeSharp", (PyObject *)&FEdgeSharp_Type);
	
	if( PyType_Ready( &FEdgeSmooth_Type ) < 0 )
		return -1;
	Py_INCREF( &FEdgeSmooth_Type );
	PyModule_AddObject(module, "FEdgeSmooth", (PyObject *)&FEdgeSmooth_Type);

	if( PyType_Ready( &Stroke_Type ) < 0 )
		return -1;
	Py_INCREF( &Stroke_Type );
	PyModule_AddObject(module, "Stroke", (PyObject *)&Stroke_Type);

	PyDict_SetItemString( Stroke_Type.tp_dict, "DRY_MEDIUM", BPy_MediumType_DRY_MEDIUM );
	PyDict_SetItemString( Stroke_Type.tp_dict, "HUMID_MEDIUM", BPy_MediumType_HUMID_MEDIUM );
	PyDict_SetItemString( Stroke_Type.tp_dict, "OPAQUE_MEDIUM", BPy_MediumType_OPAQUE_MEDIUM );

	if( PyType_Ready( &ViewEdge_Type ) < 0 )
		return -1;
	Py_INCREF( &ViewEdge_Type );
	PyModule_AddObject(module, "ViewEdge", (PyObject *)&ViewEdge_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int Interface1D___init__(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->if1D = new Interface1D();
	self->borrowed = 0;
	return 0;
}

void Interface1D___dealloc__(BPy_Interface1D* self)
{
	if( self->if1D && !self->borrowed )
		delete self->if1D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyObject * Interface1D___repr__(BPy_Interface1D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->if1D->getExactTypeName().c_str(), self->if1D );
}

PyObject *Interface1D_getExactTypeName( BPy_Interface1D *self ) {
	return PyUnicode_FromFormat( self->if1D->getExactTypeName().c_str() );	
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
	return PyLong_FromLong( self->if1D->getTimeStamp() );
}

PyObject *Interface1D_setTimeStamp( BPy_Interface1D *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, "i", &timestamp) )
		return NULL;
	
	self->if1D->setTimeStamp( timestamp );

	Py_RETURN_NONE;
}

PyObject * Interface1D_verticesBegin( BPy_Interface1D *self ) {
	Interface0DIterator if0D_it( self->if1D->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * Interface1D_verticesEnd( BPy_Interface1D *self ) {
	Interface0DIterator if0D_it( self->if1D->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}


PyObject * Interface1D_pointsBegin( BPy_Interface1D *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->if1D->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * Interface1D_pointsEnd( BPy_Interface1D *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->if1D->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


