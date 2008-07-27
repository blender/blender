#include "BPy_UnaryFunction0DDouble.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_double/BPy_Curvature2DAngleF0D.h"
#include "UnaryFunction0D_double/BPy_DensityF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedXF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedYF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedZF0D.h"
#include "UnaryFunction0D_double/BPy_GetXF0D.h"
#include "UnaryFunction0D_double/BPy_GetYF0D.h"
#include "UnaryFunction0D_double/BPy_GetZF0D.h"
#include "UnaryFunction0D_double/BPy_LocalAverageDepthF0D.h"
#include "UnaryFunction0D_double/BPy_ZDiscontinuityF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DDouble instance  -----------*/
static int UnaryFunction0DDouble___init__(BPy_UnaryFunction0DDouble* self);
static void UnaryFunction0DDouble___dealloc__(BPy_UnaryFunction0DDouble* self);
static PyObject * UnaryFunction0DDouble___repr__(BPy_UnaryFunction0DDouble* self);

static PyObject * UnaryFunction0DDouble_getName( BPy_UnaryFunction0DDouble *self);
static PyObject * UnaryFunction0DDouble___call__( BPy_UnaryFunction0DDouble *self, PyObject *args);

/*----------------------UnaryFunction0DDouble instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DDouble_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DDouble_getName, METH_NOARGS, "（ ）Returns the string of the name of the unary 0D function."},
	{"__call__", ( PyCFunction ) UnaryFunction0DDouble___call__, METH_VARARGS, "（Interface0DIterator it ）Executes the operator ()	on the iterator it pointing onto the point at which we wish to evaluate the function." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DDouble type definition ------------------------------*/

PyTypeObject UnaryFunction0DDouble_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"UnaryFunction0DDouble",				/* tp_name */
	sizeof( BPy_UnaryFunction0DDouble ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)UnaryFunction0DDouble___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)UnaryFunction0DDouble___repr__,					/* tp_repr */

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
	BPy_UnaryFunction0DDouble_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&UnaryFunction0D_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)UnaryFunction0DDouble___init__, /* initproc tp_init; */
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

PyMODINIT_FUNC UnaryFunction0DDouble_Init( PyObject *module ) {

	if( module == NULL )
		return;

	if( PyType_Ready( &UnaryFunction0DDouble_Type ) < 0 )
		return;
	Py_INCREF( &UnaryFunction0DDouble_Type );
	PyModule_AddObject(module, "UnaryFunction0DDouble", (PyObject *)&UnaryFunction0DDouble_Type);
	
	if( PyType_Ready( &DensityF0D_Type ) < 0 )
		return;
	Py_INCREF( &DensityF0D_Type );
	PyModule_AddObject(module, "DensityF0D", (PyObject *)&DensityF0D_Type);
	
	if( PyType_Ready( &LocalAverageDepthF0D_Type ) < 0 )
		return;
	Py_INCREF( &LocalAverageDepthF0D_Type );
	PyModule_AddObject(module, "LocalAverageDepthF0D", (PyObject *)&LocalAverageDepthF0D_Type);
	
	if( PyType_Ready( &Curvature2DAngleF0D_Type ) < 0 )
		return;
	Py_INCREF( &Curvature2DAngleF0D_Type );
	PyModule_AddObject(module, "Curvature2DAngleF0D", (PyObject *)&Curvature2DAngleF0D_Type);
	
	if( PyType_Ready( &GetProjectedXF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedXF0D_Type );
	PyModule_AddObject(module, "GetProjectedXF0D", (PyObject *)&GetProjectedXF0D_Type);
	
	if( PyType_Ready( &GetProjectedYF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedYF0D_Type );
	PyModule_AddObject(module, "GetProjectedYF0D", (PyObject *)&GetProjectedYF0D_Type);
	
	if( PyType_Ready( &GetProjectedZF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetProjectedZF0D_Type );
	PyModule_AddObject(module, "GetProjectedZF0D", (PyObject *)&GetProjectedZF0D_Type);
		
	if( PyType_Ready( &GetXF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetXF0D_Type );
	PyModule_AddObject(module, "GetXF0D", (PyObject *)&GetXF0D_Type);
	
	if( PyType_Ready( &GetYF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetYF0D_Type );
	PyModule_AddObject(module, "GetYF0D", (PyObject *)&GetYF0D_Type);
	
	if( PyType_Ready( &GetZF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetZF0D_Type );
	PyModule_AddObject(module, "GetZF0D", (PyObject *)&GetZF0D_Type);
	
	if( PyType_Ready( &ZDiscontinuityF0D_Type ) < 0 )
		return;
	Py_INCREF( &ZDiscontinuityF0D_Type );
	PyModule_AddObject(module, "ZDiscontinuityF0D", (PyObject *)&ZDiscontinuityF0D_Type);

		
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DDouble___init__(BPy_UnaryFunction0DDouble* self)
{
	self->uf0D_double = new UnaryFunction0D<double>();
	return 0;
}

void UnaryFunction0DDouble___dealloc__(BPy_UnaryFunction0DDouble* self)
{
	delete self->uf0D_double;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DDouble___repr__(BPy_UnaryFunction0DDouble* self)
{
	return PyString_FromFormat("type: %s - address: %p", self->uf0D_double->getName().c_str(), self->uf0D_double );
}

PyObject * UnaryFunction0DDouble_getName( BPy_UnaryFunction0DDouble *self )
{
	return PyString_FromString( self->uf0D_double->getName().c_str() );
}

PyObject * UnaryFunction0DDouble___call__( BPy_UnaryFunction0DDouble *self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_Interface0DIterator_Check(obj) ) {
		cout << "ERROR: UnaryFunction0DDouble___call__ " << endl;		
		return NULL;
	}
	
	double d = self->uf0D_double->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it ));
	return PyFloat_FromDouble( d );

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
