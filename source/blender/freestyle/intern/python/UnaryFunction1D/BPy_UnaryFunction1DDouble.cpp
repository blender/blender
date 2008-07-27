#include "BPy_UnaryFunction1DDouble.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#include "UnaryFunction1D_double/BPy_Curvature2DAngleF1D.h"
#include "UnaryFunction1D_double/BPy_DensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetCompleteViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetDirectionalViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedXF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedYF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedZF1D.h"
#include "UnaryFunction1D_double/BPy_GetSteerableViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetViewMapGradientNormF1D.h"
#include "UnaryFunction1D_double/BPy_GetXF1D.h"
#include "UnaryFunction1D_double/BPy_GetYF1D.h"
#include "UnaryFunction1D_double/BPy_GetZF1D.h"
#include "UnaryFunction1D_double/BPy_LocalAverageDepthF1D.h"
#include "UnaryFunction1D_double/BPy_ZDiscontinuityF1D.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction1DDouble instance  -----------*/
static int UnaryFunction1DDouble___init__(BPy_UnaryFunction1DDouble* self, PyObject *args);
static void UnaryFunction1DDouble___dealloc__(BPy_UnaryFunction1DDouble* self);
static PyObject * UnaryFunction1DDouble___repr__(BPy_UnaryFunction1DDouble* self);

static PyObject * UnaryFunction1DDouble_getName( BPy_UnaryFunction1DDouble *self);
static PyObject * UnaryFunction1DDouble___call__( BPy_UnaryFunction1DDouble *self, PyObject *args);
static PyObject * UnaryFunction1DDouble_setIntegrationType(BPy_UnaryFunction1DDouble* self, PyObject *args);
static PyObject * UnaryFunction1DDouble_getIntegrationType(BPy_UnaryFunction1DDouble* self);

/*----------------------UnaryFunction1DDouble instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DDouble_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DDouble_getName, METH_NOARGS, "（ ）Returns the string of the name of the unary 1D function."},
	{"__call__", ( PyCFunction ) UnaryFunction1DDouble___call__, METH_VARARGS, "（Interface1D if1D ）Builds a UnaryFunction1D from an integration type. " },
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DDouble_setIntegrationType, METH_VARARGS, "（IntegrationType i ）Sets the integration method" },
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DDouble_getIntegrationType, METH_NOARGS, "() Returns the integration method." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DDouble type definition ------------------------------*/

PyTypeObject UnaryFunction1DDouble_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"UnaryFunction1DDouble",				/* tp_name */
	sizeof( BPy_UnaryFunction1DDouble ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)UnaryFunction1DDouble___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)UnaryFunction1DDouble___repr__,					/* tp_repr */

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
	BPy_UnaryFunction1DDouble_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&UnaryFunction1D_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)UnaryFunction1DDouble___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	NULL,		/* newfunc tp_new; */
	
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

PyMODINIT_FUNC UnaryFunction1DDouble_Init( PyObject *module ) {

	if( module == NULL )
		return;

	if( PyType_Ready( &UnaryFunction1DDouble_Type ) < 0 )
		return;
	Py_INCREF( &UnaryFunction1DDouble_Type );
	PyModule_AddObject(module, "UnaryFunction1DDouble", (PyObject *)&UnaryFunction1DDouble_Type);

	if( PyType_Ready( &DensityF1D_Type ) < 0 )
		return;
	Py_INCREF( &DensityF1D_Type );
	PyModule_AddObject(module, "DensityF1D", (PyObject *)&DensityF1D_Type);

	if( PyType_Ready( &GetCompleteViewMapDensityF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetCompleteViewMapDensityF1D_Type );
	PyModule_AddObject(module, "GetCompleteViewMapDensityF1D", (PyObject *)&GetCompleteViewMapDensityF1D_Type);

	if( PyType_Ready( &GetDirectionalViewMapDensityF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetDirectionalViewMapDensityF1D_Type );
	PyModule_AddObject(module, "GetDirectionalViewMapDensityF1D", (PyObject *)&GetDirectionalViewMapDensityF1D_Type);

	if( PyType_Ready( &GetProjectedXF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedXF1D_Type );
	PyModule_AddObject(module, "GetProjectedXF1D", (PyObject *)&GetProjectedXF1D_Type);

	if( PyType_Ready( &GetProjectedYF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedYF1D_Type );
	PyModule_AddObject(module, "GetProjectedYF1D", (PyObject *)&GetProjectedYF1D_Type);
	
	if( PyType_Ready( &GetProjectedZF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedZF1D_Type );
	PyModule_AddObject(module, "GetProjectedZF1D", (PyObject *)&GetProjectedZF1D_Type);

	if( PyType_Ready( &GetSteerableViewMapDensityF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetSteerableViewMapDensityF1D_Type );
	PyModule_AddObject(module, "GetSteerableViewMapDensityF1D", (PyObject *)&GetSteerableViewMapDensityF1D_Type);

	if( PyType_Ready( &GetViewMapGradientNormF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetViewMapGradientNormF1D_Type );
	PyModule_AddObject(module, "GetViewMapGradientNormF1D", (PyObject *)&GetViewMapGradientNormF1D_Type);

	if( PyType_Ready( &GetXF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetXF1D_Type );
	PyModule_AddObject(module, "GetXF1D", (PyObject *)&GetXF1D_Type);

	if( PyType_Ready( &GetYF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetYF1D_Type );
	PyModule_AddObject(module, "GetYF1D", (PyObject *)&GetYF1D_Type);

	if( PyType_Ready( &GetZF1D_Type ) < 0 )
		return;
	Py_INCREF( &GetZF1D_Type );
	PyModule_AddObject(module, "GetZF1D", (PyObject *)&GetZF1D_Type);

	if( PyType_Ready( &LocalAverageDepthF1D_Type ) < 0 )
		return;
	Py_INCREF( &LocalAverageDepthF1D_Type );
	PyModule_AddObject(module, "LocalAverageDepthF1D", (PyObject *)&LocalAverageDepthF1D_Type);

	if( PyType_Ready( &ZDiscontinuityF1D_Type ) < 0 )
		return;
	Py_INCREF( &ZDiscontinuityF1D_Type );
	PyModule_AddObject(module, "ZDiscontinuityF1D", (PyObject *)&ZDiscontinuityF1D_Type);
		
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction1DDouble___init__(BPy_UnaryFunction1DDouble* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "|O", &obj) && BPy_IntegrationType_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DDouble___init__ " << endl;		
		return -1;
	}
	
	if( !obj )
		self->uf1D_double = new UnaryFunction1D<double>();
	else {
		self->uf1D_double = new UnaryFunction1D<double>( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	return 0;
}

void UnaryFunction1DDouble___dealloc__(BPy_UnaryFunction1DDouble* self)
{
	delete self->uf1D_double;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction1DDouble___repr__(BPy_UnaryFunction1DDouble* self)
{
	return PyString_FromFormat("type: %s - address: %p", self->uf1D_double->getName().c_str(), self->uf1D_double );
}

PyObject * UnaryFunction1DDouble_getName( BPy_UnaryFunction1DDouble *self )
{
	return PyString_FromString( self->uf1D_double->getName().c_str() );
}

PyObject * UnaryFunction1DDouble___call__( BPy_UnaryFunction1DDouble *self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_Interface1D_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DDouble___call__ " << endl;		
		return NULL;
	}
	
	double d = self->uf1D_double->operator()(*( ((BPy_Interface1D *) obj)->if1D ));
	return PyFloat_FromDouble( d );

}

PyObject * UnaryFunction1DDouble_setIntegrationType(BPy_UnaryFunction1DDouble* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_IntegrationType_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DDouble_setIntegrationType " << endl;		
		Py_RETURN_NONE;
	}
	
	self->uf1D_double->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

PyObject * UnaryFunction1DDouble_getIntegrationType(BPy_UnaryFunction1DDouble* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_double->getIntegrationType() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
