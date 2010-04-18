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

static char Interface0D___doc__[] =
"Base class for any 0D element.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n";

static int Interface0D___init__(BPy_Interface0D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->if0D = new Interface0D();
	self->borrowed = 0;
	return 0;
}

static void Interface0D___dealloc__(BPy_Interface0D* self)
{
	if( self->if0D && !self->borrowed )
		delete self->if0D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * Interface0D___repr__(BPy_Interface0D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->if0D->getExactTypeName().c_str(), self->if0D );
}

static char Interface0D_getExactTypeName___doc__[] =
".. method:: getExactTypeName()\n"
"\n"
"   Returns the name of the 0D element.\n"
"\n"
"   :return: Name of the interface.\n"
"   :rtype: string\n";

static PyObject *Interface0D_getExactTypeName( BPy_Interface0D *self ) {
	return PyUnicode_FromFormat( self->if0D->getExactTypeName().c_str() );	
}

static char Interface0D_getX___doc__[] =
".. method:: getX()\n"
"\n"
"   Returns the X coordinate of the 3D point of the 0D element.\n"
"\n"
"   :return: The X coordinate of the 3D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getX( BPy_Interface0D *self ) {
	double x = self->if0D->getX();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( x );
}

static char Interface0D_getY___doc__[] =
".. method:: getY()\n"
"\n"
"   Returns the Y coordinate of the 3D point of the 0D element.\n"
"\n"
"   :return: The Y coordinate of the 3D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getY( BPy_Interface0D *self ) {
	double y = self->if0D->getY();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( y );
}

static char Interface0D_getZ___doc__[] =
".. method:: getZ()\n"
"\n"
"   Returns the Z coordinate of the 3D point of the 0D element.\n"
"\n"
"   :return: The Z coordinate of the 3D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getZ( BPy_Interface0D *self ) {
	double z = self->if0D->getZ();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( z );
}

static char Interface0D_getPoint3D___doc__[] =
".. method:: getPoint3D()\n"
"\n"
"   Returns the location of the 0D element in the 3D space.\n"
"\n"
"   :return: The 3D point of the 0D element.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *Interface0D_getPoint3D( BPy_Interface0D *self ) {
	Vec3f v( self->if0D->getPoint3D() );
	if (PyErr_Occurred())
		return NULL;
	return Vector_from_Vec3f( v );
}

static char Interface0D_getProjectedX___doc__[] =
".. method:: getProjectedX()\n"
"\n"
"   Returns the X coordinate of the 2D point of the 0D element.\n"
"\n"
"   :return: The X coordinate of the 2D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getProjectedX( BPy_Interface0D *self ) {
	double x = self->if0D->getProjectedX();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( x );
}

static char Interface0D_getProjectedY___doc__[] =
".. method:: getProjectedY()\n"
"\n"
"   Returns the Y coordinate of the 2D point of the 0D element.\n"
"\n"
"   :return: The Y coordinate of the 2D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getProjectedY( BPy_Interface0D *self ) {
	double y = self->if0D->getProjectedY();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( y );
}

static char Interface0D_getProjectedZ___doc__[] =
".. method:: getProjectedZ()\n"
"\n"
"   Returns the Z coordinate of the 2D point of the 0D element.\n"
"\n"
"   :return: The Z coordinate of the 2D point.\n"
"   :rtype: float\n";

static PyObject *Interface0D_getProjectedZ( BPy_Interface0D *self ) {
	double z = self->if0D->getProjectedZ();
	if (PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble( z );
}

static char Interface0D_getPoint2D___doc__[] =
".. method:: getPoint2D()\n"
"\n"
"   Returns the location of the 0D element in the 2D space.\n"
"\n"
"   :return: The 2D point of the 0D element.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *Interface0D_getPoint2D( BPy_Interface0D *self ) {
	Vec2f v( self->if0D->getPoint2D() );
	if (PyErr_Occurred())
		return NULL;
	return Vector_from_Vec2f( v );
}

static char Interface0D_getFEdge___doc__[] =
".. method:: getFEdge(inter)\n"
"\n"
"   Returns the FEdge that lies between this 0D element and the 0D\n"
"   element given as the argument.\n"
"\n"
"   :arg inter: A 0D element.\n"
"   :type inter: :class:`Interface0D`\n"
"   :return: The FEdge lying between the two 0D elements.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject *Interface0D_getFEdge( BPy_Interface0D *self, PyObject *args ) {
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

static char Interface0D_getId___doc__[] =
".. method:: getId()\n"
"\n"
"   Returns the identifier of the 0D element.\n"
"\n"
"   :return: The identifier of the 0D element.\n"
"   :rtype: :class:`Id`\n";

static PyObject *Interface0D_getId( BPy_Interface0D *self ) {
	Id id( self->if0D->getId() );
	if (PyErr_Occurred())
		return NULL;
	return BPy_Id_from_Id( id );
}

static char Interface0D_getNature___doc__[] =
".. method:: getNature()\n"
"\n"
"   Returns the nature of the 0D element.\n"
"\n"
"   :return: The nature of the 0D element.\n"
"   :rtype: :class:`Nature`\n";

static PyObject *Interface0D_getNature( BPy_Interface0D *self ) {
	Nature::VertexNature nature = self->if0D->getNature();
	if (PyErr_Occurred())
		return NULL;
	return BPy_Nature_from_Nature( nature );
}

/*----------------------Interface0D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface0D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface0D_getExactTypeName, METH_NOARGS, Interface0D_getExactTypeName___doc__},
	{"getX", ( PyCFunction ) Interface0D_getX, METH_NOARGS, Interface0D_getX___doc__},
	{"getY", ( PyCFunction ) Interface0D_getY, METH_NOARGS, Interface0D_getY___doc__},
	{"getZ", ( PyCFunction ) Interface0D_getZ, METH_NOARGS, Interface0D_getZ___doc__},
	{"getPoint3D", ( PyCFunction ) Interface0D_getPoint3D, METH_NOARGS, Interface0D_getPoint3D___doc__},
	{"getProjectedX", ( PyCFunction ) Interface0D_getProjectedX, METH_NOARGS, Interface0D_getProjectedX___doc__},
	{"getProjectedY", ( PyCFunction ) Interface0D_getProjectedY, METH_NOARGS, Interface0D_getProjectedY___doc__},
	{"getProjectedZ", ( PyCFunction ) Interface0D_getProjectedZ, METH_NOARGS, Interface0D_getProjectedZ___doc__},
	{"getPoint2D", ( PyCFunction ) Interface0D_getPoint2D, METH_NOARGS, Interface0D_getPoint2D___doc__},
	{"getFEdge", ( PyCFunction ) Interface0D_getFEdge, METH_VARARGS, Interface0D_getFEdge___doc__},
	{"getId", ( PyCFunction ) Interface0D_getId, METH_NOARGS, Interface0D_getId___doc__},
	{"getNature", ( PyCFunction ) Interface0D_getNature, METH_NOARGS, Interface0D_getNature___doc__},
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
	Interface0D___doc__,            /* tp_doc */
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
	0,				/* tp_free */
	0,				/* tp_is_gc */
	0,				/* tp_bases */
	0,				/* tp_mro */
	0,				/* tp_cache */
	0,				/* tp_subclasses */
	0,				/* tp_weaklist */
	0				/* tp_del */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

