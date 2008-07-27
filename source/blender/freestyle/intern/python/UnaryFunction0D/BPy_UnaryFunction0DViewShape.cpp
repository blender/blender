#include "BPy_UnaryFunction0DViewShape.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_ViewShape/BPy_GetOccludeeF0D.h"
#include "UnaryFunction0D_ViewShape/BPy_GetShapeF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DViewShape instance  -----------*/
static int UnaryFunction0DViewShape___init__(BPy_UnaryFunction0DViewShape* self);
static void UnaryFunction0DViewShape___dealloc__(BPy_UnaryFunction0DViewShape* self);
static PyObject * UnaryFunction0DViewShape___repr__(BPy_UnaryFunction0DViewShape* self);

static PyObject * UnaryFunction0DViewShape_getName( BPy_UnaryFunction0DViewShape *self);
static PyObject * UnaryFunction0DViewShape___call__( BPy_UnaryFunction0DViewShape *self, PyObject *args);

/*----------------------UnaryFunction0DViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DViewShape_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DViewShape_getName, METH_NOARGS, "（ ）Returns the string of the name of the unary 0D function."},
	{"__call__", ( PyCFunction ) UnaryFunction0DViewShape___call__, METH_VARARGS, "（Interface0DIterator it ）Executes the operator ()	on the iterator it pointing onto the point at which we wish to evaluate the function." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DViewShape type definition ------------------------------*/

PyTypeObject UnaryFunction0DViewShape_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"UnaryFunction0DViewShape",				/* tp_name */
	sizeof( BPy_UnaryFunction0DViewShape ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)UnaryFunction0DViewShape___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)UnaryFunction0DViewShape___repr__,					/* tp_repr */

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
	BPy_UnaryFunction0DViewShape_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&UnaryFunction0D_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)UnaryFunction0DViewShape___init__, /* initproc tp_init; */
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

PyMODINIT_FUNC UnaryFunction0DViewShape_Init( PyObject *module ) {

	if( module == NULL )
		return;

	if( PyType_Ready( &UnaryFunction0DViewShape_Type ) < 0 )
		return;
	Py_INCREF( &UnaryFunction0DViewShape_Type );
	PyModule_AddObject(module, "UnaryFunction0DViewShape", (PyObject *)&UnaryFunction0DViewShape_Type);

	if( PyType_Ready( &GetOccludeeF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetOccludeeF0D_Type );
	PyModule_AddObject(module, "GetOccludeeF0D", (PyObject *)&GetOccludeeF0D_Type);
	
	if( PyType_Ready( &GetShapeF0D_Type ) < 0 )
		return;
	Py_INCREF( &GetShapeF0D_Type );
	PyModule_AddObject(module, "GetShapeF0D", (PyObject *)&GetShapeF0D_Type);
		
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DViewShape___init__(BPy_UnaryFunction0DViewShape* self)
{
	self->uf0D_viewshape = new UnaryFunction0D<ViewShape*>();
	return 0;
}

void UnaryFunction0DViewShape___dealloc__(BPy_UnaryFunction0DViewShape* self)
{
	delete self->uf0D_viewshape;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DViewShape___repr__(BPy_UnaryFunction0DViewShape* self)
{
	return PyString_FromFormat("type: %s - address: %p", self->uf0D_viewshape->getName().c_str(), self->uf0D_viewshape );
}

PyObject * UnaryFunction0DViewShape_getName( BPy_UnaryFunction0DViewShape *self )
{
	return PyString_FromString( self->uf0D_viewshape->getName().c_str() );
}

PyObject * UnaryFunction0DViewShape___call__( BPy_UnaryFunction0DViewShape *self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_Interface0DIterator_Check(obj) ) {
		cout << "ERROR: UnaryFunction0DViewShape___call__ " << endl;		
		return NULL;
	}
	
	ViewShape *vs = self->uf0D_viewshape->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it ));
	return BPy_ViewShape_from_ViewShape( *vs );

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
