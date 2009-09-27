#include "BPy_Interface0D.h"

#include "BPy_Convert.h"
#include "Interface0D/BPy_CurvePoint.h"
#include "Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface0D/ViewVertex/BPy_NonTVertex.h"
#include "Interface0D/ViewVertex/BPy_TVertex.h"
#include "Interface1D/BPy_FEdge.h"
#include "BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Interface0D instance  -----------*/
static int Interface0D___init__(BPy_Interface0D *self, PyObject *args, PyObject *kwds);
static void Interface0D___dealloc__(BPy_Interface0D *self);
static PyObject * Interface0D___repr__(BPy_Interface0D *self);

static PyObject *Interface0D_getExactTypeName( BPy_Interface0D *self );
static PyObject *Interface0D_getX( BPy_Interface0D *self );
static PyObject *Interface0D_getY( BPy_Interface0D *self );
static PyObject *Interface0D_getZ( BPy_Interface0D *self );
static PyObject *Interface0D_getPoint3D( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedX( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedY( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedZ( BPy_Interface0D *self );
static PyObject *Interface0D_getPoint2D( BPy_Interface0D *self );
static PyObject *Interface0D_getFEdge( BPy_Interface0D *self, PyObject *args );
static PyObject *Interface0D_getId( BPy_Interface0D *self );
static PyObject *Interface0D_getNature( BPy_Interface0D *self ); 

/*----------------------Interface0D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface0D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface0D_getExactTypeName, METH_NOARGS, "() Returns the string of the name of the interface."},
	{"getX", ( PyCFunction ) Interface0D_getX, METH_NOARGS, "() Returns the 3D x coordinate of the point. "},
	{"getY", ( PyCFunction ) Interface0D_getY, METH_NOARGS, "() Returns the 3D y coordinate of the point. "},
	{"getZ", ( PyCFunction ) Interface0D_getZ, METH_NOARGS, "() Returns the 3D z coordinate of the point. "},
	{"getPoint3D", ( PyCFunction ) Interface0D_getPoint3D, METH_NOARGS, "() Returns the 3D point."},
	{"getProjectedX", ( PyCFunction ) Interface0D_getProjectedX, METH_NOARGS, "() Returns the 2D x coordinate of the point."},
	{"getProjectedY", ( PyCFunction ) Interface0D_getProjectedY, METH_NOARGS, "() Returns the 2D y coordinate of the point."},
	{"getProjectedZ", ( PyCFunction ) Interface0D_getProjectedZ, METH_NOARGS, "() Returns the 2D z coordinate of the point."},
	{"getPoint2D", ( PyCFunction ) Interface0D_getPoint2D, METH_NOARGS, "() Returns the 2D point."},
	{"getFEdge", ( PyCFunction ) Interface0D_getFEdge, METH_VARARGS, "(Interface0D i) Returns the FEdge that lies between this Interface0D and the Interface0D given as argument."},
	{"getId", ( PyCFunction ) Interface0D_getId, METH_NOARGS, "() Returns the Id of the point."},
	{"getNature", ( PyCFunction ) Interface0D_getNature, METH_NOARGS, "() Returns the nature of the point."},	
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface0D type definition ------------------------------*/

PyTypeObject Interface0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Interface0D",                  /* tp_name */
	sizeof(BPy_Interface0D),        /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Interface0D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)Interface0D___repr__, /* tp_repr */
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
	"Interface0D objects",          /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Interface0D_methods,        /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Interface0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int Interface0D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &Interface0D_Type ) < 0 )
		return -1;
	Py_INCREF( &Interface0D_Type );
	PyModule_AddObject(module, "Interface0D", (PyObject *)&Interface0D_Type);
	
	if( PyType_Ready( &CurvePoint_Type ) < 0 )
		return -1;
	Py_INCREF( &CurvePoint_Type );
	PyModule_AddObject(module, "CurvePoint", (PyObject *)&CurvePoint_Type);
	
	if( PyType_Ready( &SVertex_Type ) < 0 )
		return -1;
	Py_INCREF( &SVertex_Type );
	PyModule_AddObject(module, "SVertex", (PyObject *)&SVertex_Type);	
	
	if( PyType_Ready( &ViewVertex_Type ) < 0 )
		return -1;
	Py_INCREF( &ViewVertex_Type );
	PyModule_AddObject(module, "ViewVertex", (PyObject *)&ViewVertex_Type);
	
	if( PyType_Ready( &StrokeVertex_Type ) < 0 )
		return -1;
	Py_INCREF( &StrokeVertex_Type );
	PyModule_AddObject(module, "StrokeVertex", (PyObject *)&StrokeVertex_Type);
	
	if( PyType_Ready( &NonTVertex_Type ) < 0 )
		return -1;
	Py_INCREF( &NonTVertex_Type );
	PyModule_AddObject(module, "NonTVertex", (PyObject *)&NonTVertex_Type);
	
	if( PyType_Ready( &TVertex_Type ) < 0 )
		return -1;
	Py_INCREF( &TVertex_Type );
	PyModule_AddObject(module, "TVertex", (PyObject *)&TVertex_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int Interface0D___init__(BPy_Interface0D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->if0D = new Interface0D();
	self->borrowed = 0;
	return 0;
}

void Interface0D___dealloc__(BPy_Interface0D* self)
{
	if( self->if0D && !self->borrowed )
		delete self->if0D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyObject * Interface0D___repr__(BPy_Interface0D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->if0D->getExactTypeName().c_str(), self->if0D );
}

PyObject *Interface0D_getExactTypeName( BPy_Interface0D *self ) {
	return PyUnicode_FromFormat( self->if0D->getExactTypeName().c_str() );	
}


PyObject *Interface0D_getX( BPy_Interface0D *self ) {
	double x = self->if0D->getX();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( x );
}


PyObject *Interface0D_getY( BPy_Interface0D *self ) {
	double y = self->if0D->getY();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( y );
}


PyObject *Interface0D_getZ( BPy_Interface0D *self ) {
	double z = self->if0D->getZ();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( z );
}


PyObject *Interface0D_getPoint3D( BPy_Interface0D *self ) {
	Vec3f v( self->if0D->getPoint3D() );
	if (PyErr_Occurred())
		return NULL;
	return Vector_from_Vec3f( v );
}


PyObject *Interface0D_getProjectedX( BPy_Interface0D *self ) {
	double x = self->if0D->getProjectedX();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( x );
}


PyObject *Interface0D_getProjectedY( BPy_Interface0D *self ) {
	double y = self->if0D->getProjectedY();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( y );
}


PyObject *Interface0D_getProjectedZ( BPy_Interface0D *self ) {
	double z = self->if0D->getProjectedZ();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( z );
}


PyObject *Interface0D_getPoint2D( BPy_Interface0D *self ) {
	Vec2f v( self->if0D->getPoint2D() );
	if (PyErr_Occurred())
		return NULL;
	return Vector_from_Vec2f( v );
}


PyObject *Interface0D_getFEdge( BPy_Interface0D *self, PyObject *args ) {
	PyObject *py_if0D;

	if(!( PyArg_ParseTuple(args, "O!", &Interface0D_Type, &py_if0D) ))
		return NULL;

	FEdge *fe = self->if0D->getFEdge(*( ((BPy_Interface0D *) py_if0D)->if0D ));
	if (PyErr_Occurred())
		return NULL;
	if( fe )
		return Any_BPy_FEdge_from_FEdge( *fe );
	
	Py_RETURN_NONE;
}


PyObject *Interface0D_getId( BPy_Interface0D *self ) {
	Id id( self->if0D->getId() );
	if (PyErr_Occurred())
		return NULL;
	return BPy_Id_from_Id( id );
}


PyObject *Interface0D_getNature( BPy_Interface0D *self ) {
	Nature::VertexNature nature = self->if0D->getNature();
	if (PyErr_Occurred())
		return NULL;
	return BPy_Nature_from_Nature( nature );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

