#include "BPy_UnaryFunction1DVoid.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#include "UnaryFunction1D_void/BPy_ChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_IncrementChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_TimeStampF1D.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction1DVoid instance  -----------*/
static int UnaryFunction1DVoid___init__(BPy_UnaryFunction1DVoid* self, PyObject *args);
static void UnaryFunction1DVoid___dealloc__(BPy_UnaryFunction1DVoid* self);
static PyObject * UnaryFunction1DVoid___repr__(BPy_UnaryFunction1DVoid* self);

static PyObject * UnaryFunction1DVoid_getName( BPy_UnaryFunction1DVoid *self);
static PyObject * UnaryFunction1DVoid___call__( BPy_UnaryFunction1DVoid *self, PyObject *args);

static PyObject * UnaryFunction1DVoid_setIntegrationType(BPy_UnaryFunction1DVoid* self, PyObject *args);
static PyObject * UnaryFunction1DVoid_getIntegrationType(BPy_UnaryFunction1DVoid* self);

/*----------------------UnaryFunction1DVoid instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DVoid_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DVoid_getName, METH_NOARGS, "（ ）Returns the string of the name of the unary 1D function."},
	{"__call__", ( PyCFunction ) UnaryFunction1DVoid___call__, METH_VARARGS, "（Interface1D if1D ）Builds a UnaryFunction1D from an integration type. " },
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DVoid_setIntegrationType, METH_VARARGS, "（IntegrationType i ）Sets the integration method" },
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DVoid_getIntegrationType, METH_NOARGS, "() Returns the integration method." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DVoid type definition ------------------------------*/

PyTypeObject UnaryFunction1DVoid_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"UnaryFunction1DVoid",				/* tp_name */
	sizeof( BPy_UnaryFunction1DVoid ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)UnaryFunction1DVoid___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)UnaryFunction1DVoid___repr__,					/* tp_repr */

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
	BPy_UnaryFunction1DVoid_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&UnaryFunction1D_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)UnaryFunction1DVoid___init__, /* initproc tp_init; */
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

PyMODINIT_FUNC UnaryFunction1DVoid_Init( PyObject *module ) {

	if( module == NULL )
		return;

	if( PyType_Ready( &UnaryFunction1DVoid_Type ) < 0 )
		return;
	Py_INCREF( &UnaryFunction1DVoid_Type );
	PyModule_AddObject(module, "UnaryFunction1DVoid", (PyObject *)&UnaryFunction1DVoid_Type);
	
	if( PyType_Ready( &ChainingTimeStampF1D_Type ) < 0 )
		return;
	Py_INCREF( &ChainingTimeStampF1D_Type );
	PyModule_AddObject(module, "ChainingTimeStampF1D", (PyObject *)&ChainingTimeStampF1D_Type);
	
	if( PyType_Ready( &IncrementChainingTimeStampF1D_Type ) < 0 )
		return;
	Py_INCREF( &IncrementChainingTimeStampF1D_Type );
	PyModule_AddObject(module, "IncrementChainingTimeStampF1D", (PyObject *)&IncrementChainingTimeStampF1D_Type);
	
	if( PyType_Ready( &TimeStampF1D_Type ) < 0 )
		return;
	Py_INCREF( &TimeStampF1D_Type );
	PyModule_AddObject(module, "TimeStampF1D", (PyObject *)&TimeStampF1D_Type);
	
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction1DVoid___init__(BPy_UnaryFunction1DVoid* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "|O", &obj) && BPy_IntegrationType_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DVoid___init__ " << endl;		
		return -1;
	}
	
	if( !obj )
		self->uf1D_void = new UnaryFunction1D_void();
	else {
		self->uf1D_void = new UnaryFunction1D_void( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	self->uf1D_void->py_uf1D = (PyObject *)self;
	
	return 0;
}

void UnaryFunction1DVoid___dealloc__(BPy_UnaryFunction1DVoid* self)
{
	delete self->uf1D_void;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction1DVoid___repr__(BPy_UnaryFunction1DVoid* self)
{
	return PyString_FromFormat("type: %s - address: %p", self->uf1D_void->getName().c_str(), self->uf1D_void );
}

PyObject * UnaryFunction1DVoid_getName( BPy_UnaryFunction1DVoid *self )
{
	return PyString_FromString( self->uf1D_void->getName().c_str() );
}

PyObject * UnaryFunction1DVoid___call__( BPy_UnaryFunction1DVoid *self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_Interface1D_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DVoid___call__ " << endl;		
		return NULL;
	}
	
	self->uf1D_void->operator()(*( ((BPy_Interface1D *) obj)->if1D ));
	Py_RETURN_NONE;
}

PyObject * UnaryFunction1DVoid_setIntegrationType(BPy_UnaryFunction1DVoid* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_IntegrationType_Check(obj) ) {
		cout << "ERROR: UnaryFunction1DVoid_setIntegrationType " << endl;		
		Py_RETURN_NONE;
	}
	
	self->uf1D_void->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

PyObject * UnaryFunction1DVoid_getIntegrationType(BPy_UnaryFunction1DVoid* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_void->getIntegrationType() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
