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

static char Interface1D___doc__[] =
"Base class for any 1D element.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n";

static int Interface1D___init__(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->if1D = new Interface1D();
	self->borrowed = 0;
	return 0;
}

static void Interface1D___dealloc__(BPy_Interface1D* self)
{
	if( self->if1D && !self->borrowed )
		delete self->if1D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * Interface1D___repr__(BPy_Interface1D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->if1D->getExactTypeName().c_str(), self->if1D );
}

static char Interface1D_getExactTypeName___doc__[] =
".. method:: getExactTypeName()\n"
"\n"
"   Returns the string of the name of the 1D element.\n"
"\n"
"   :return: The name of the 1D element.\n"
"   :rtype: string\n";

static PyObject *Interface1D_getExactTypeName( BPy_Interface1D *self ) {
	return PyUnicode_FromFormat( self->if1D->getExactTypeName().c_str() );	
}

#if 0
static PyObject *Interface1D_getVertices( BPy_Interface1D *self ) {
	return PyList_New(0);
}

static PyObject *Interface1D_getPoints( BPy_Interface1D *self ) {
	return PyList_New(0);
}
#endif

static char Interface1D_getLength2D___doc__[] =
".. method:: getLength2D()\n"
"\n"
"   Returns the 2D length of the 1D element.\n"
"\n"
"   :return: The 2D length of the 1D element.\n"
"   :rtype: float\n";

static PyObject *Interface1D_getLength2D( BPy_Interface1D *self ) {
	return PyFloat_FromDouble( (double) self->if1D->getLength2D() );
}

static char Interface1D_getId___doc__[] =
".. method:: getId()\n"
"\n"
"   Returns the Id of the 1D element .\n"
"\n"
"   :return: The Id of the 1D element .\n"
"   :rtype: :class:`Id`\n";

static PyObject *Interface1D_getId( BPy_Interface1D *self ) {
	Id id( self->if1D->getId() );
	return BPy_Id_from_Id( id );
}

static char Interface1D_getNature___doc__[] =
".. method:: getNature()\n"
"\n"
"   Returns the nature of the 1D element.\n"
"\n"
"   :return: The nature of the 1D element.\n"
"   :rtype: :class:`Nature`\n";

static PyObject *Interface1D_getNature( BPy_Interface1D *self ) {
	return BPy_Nature_from_Nature( self->if1D->getNature() );
}

static char Interface1D_getTimeStamp___doc__[] =
".. method:: getTimeStamp()\n"
"\n"
"   Returns the time stamp of the 1D element. Mainly used for selection.\n"
"\n"
"   :return: The time stamp of the 1D element.\n"
"   :rtype: int\n";

static PyObject *Interface1D_getTimeStamp( BPy_Interface1D *self ) {
	return PyLong_FromLong( self->if1D->getTimeStamp() );
}

static char Interface1D_setTimeStamp___doc__[] =
".. method:: setTimeStamp(iTimeStamp)\n"
"\n"
"   Sets the time stamp for the 1D element.\n"
"\n"
"   :arg iTimeStamp: A time stamp.\n"
"   :type iTimeStamp: int\n";

static PyObject *Interface1D_setTimeStamp( BPy_Interface1D *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, "i", &timestamp) )
		return NULL;
	
	self->if1D->setTimeStamp( timestamp );

	Py_RETURN_NONE;
}

static char Interface1D_verticesBegin___doc__[] =
".. method:: verticesBegin()\n"
"\n"
"   Returns an iterator over the Interface1D vertices, pointing to the\n"
"   first vertex.\n"
"\n"
"   :return: An Interface0DIterator pointing to the first vertex.\n"
"   :rtype: :class:`Interface0DIterator`\n";

static PyObject * Interface1D_verticesBegin( BPy_Interface1D *self ) {
	Interface0DIterator if0D_it( self->if1D->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

static char Interface1D_verticesEnd___doc__[] =
".. method:: verticesEnd()\n"
"\n"
"   Returns an iterator over the Interface1D vertices, pointing after\n"
"   the last vertex.\n"
"\n"
"   :return: An Interface0DIterator pointing after the last vertex.\n"
"   :rtype: :class:`Interface0DIterator`\n";

static PyObject * Interface1D_verticesEnd( BPy_Interface1D *self ) {
	Interface0DIterator if0D_it( self->if1D->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}

static char Interface1D_pointsBegin___doc__[] =
".. method:: pointsBegin(t=0.0)\n"
"\n"
"   Returns an iterator over the Interface1D points, pointing to the\n"
"   first point. The difference with verticesBegin() is that here we can\n"
"   iterate over points of the 1D element at a any given sampling.\n"
"   Indeed, for each iteration, a virtual point is created.\n"
"\n"
"   :arg t: A sampling with which we want to iterate over points of\n"
"      this 1D element.\n"
"   :type t: float\n"
"   :return: An Interface0DIterator pointing to the first point.\n"
"   :rtype: :class:`Interface0DIterator`\n";

static PyObject * Interface1D_pointsBegin( BPy_Interface1D *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->if1D->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

static char Interface1D_pointsEnd___doc__[] =
".. method:: pointsEnd(t=0.0)\n"
"\n"
"   Returns an iterator over the Interface1D points, pointing after the\n"
"   last point. The difference with verticesEnd() is that here we can\n"
"   iterate over points of the 1D element at a given sampling.  Indeed,\n"
"   for each iteration, a virtual point is created.\n"
"\n"
"   :arg t: A sampling with which we want to iterate over points of\n"
"      this 1D element.\n"
"   :type t: float\n"
"   :return: An Interface0DIterator pointing after the last point.\n"
"   :rtype: :class:`Interface0DIterator`\n";

static PyObject * Interface1D_pointsEnd( BPy_Interface1D *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->if1D->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}

/*----------------------Interface1D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface1D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface1D_getExactTypeName, METH_NOARGS, Interface1D_getExactTypeName___doc__},
#if 0
	{"getVertices", ( PyCFunction ) Interface1D_getVertices, METH_NOARGS, "Returns the vertices"},
	{"getPoints", ( PyCFunction ) Interface1D_getPoints, METH_NOARGS, "Returns the points. The difference with getVertices() is that here we can iterate over points of the 1D element at any given sampling. At each call, a virtual point is created."},
#endif
	{"getLength2D", ( PyCFunction ) Interface1D_getLength2D, METH_NOARGS, Interface1D_getLength2D___doc__},
	{"getId", ( PyCFunction ) Interface1D_getId, METH_NOARGS, Interface1D_getId___doc__},
	{"getNature", ( PyCFunction ) Interface1D_getNature, METH_NOARGS, Interface1D_getNature___doc__},
	{"getTimeStamp", ( PyCFunction ) Interface1D_getTimeStamp, METH_NOARGS, Interface1D_getTimeStamp___doc__},
	{"setTimeStamp", ( PyCFunction ) Interface1D_setTimeStamp, METH_VARARGS, Interface1D_setTimeStamp___doc__},
	{"verticesBegin", ( PyCFunction ) Interface1D_verticesBegin, METH_NOARGS, Interface1D_verticesBegin___doc__},
	{"verticesEnd", ( PyCFunction ) Interface1D_verticesEnd, METH_NOARGS, Interface1D_verticesEnd___doc__},
	{"pointsBegin", ( PyCFunction ) Interface1D_pointsBegin, METH_VARARGS, Interface1D_pointsBegin___doc__},
	{"pointsEnd", ( PyCFunction ) Interface1D_pointsEnd, METH_VARARGS, Interface1D_pointsEnd___doc__},
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
	Interface1D___doc__,            /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


