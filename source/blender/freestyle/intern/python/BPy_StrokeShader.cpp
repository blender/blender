#include "BPy_StrokeShader.h"

#include "BPy_Convert.h"
#include "Interface1D/BPy_Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeShader instance  -----------*/
static int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds);
static void StrokeShader___dealloc__(BPy_StrokeShader *self);
static PyObject * StrokeShader___repr__(BPy_StrokeShader *self);

static PyObject * StrokeShader_getName( BPy_StrokeShader *self, PyObject *args);
static PyObject * StrokeShader_shade( BPy_StrokeShader *self , PyObject *args);

/*----------------------StrokeShader instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeShader_methods[] = {
	{"getName", ( PyCFunction ) StrokeShader_getName, METH_NOARGS, "（ ）Returns the string of the name of the binary predicate."},
	{"shade", ( PyCFunction ) StrokeShader_shade, METH_VARARGS, "（Stroke s ）The shading method. This method must be overloaded by inherited classes. The shading method is designed to modify any Stroke's attribute such as Thickness, Color, Geometry, Texture, Blending mode... The basic way to achieve this operation consists in iterating over the StrokeVertices of the Stroke and to modify each one's StrokeAttribute."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeShader type definition ------------------------------*/

PyTypeObject StrokeShader_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"StrokeShader",				/* tp_name */
	sizeof( BPy_StrokeShader ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)StrokeShader___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)StrokeShader___repr__,					/* tp_repr */

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
	BPy_StrokeShader_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)StrokeShader___init__, /* initproc tp_init; */
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
PyMODINIT_FUNC StrokeShader_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &StrokeShader_Type ) < 0 )
		return;

	Py_INCREF( &StrokeShader_Type );
	PyModule_AddObject(module, "StrokeShader", (PyObject *)&StrokeShader_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds)
{
	self->ss = new StrokeShader();
	return 0;
}

void StrokeShader___dealloc__(BPy_StrokeShader* self)
{
	delete self->ss;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * StrokeShader___repr__(BPy_StrokeShader* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->ss->getName().c_str(), self->ss );
}


PyObject * StrokeShader_getName( BPy_StrokeShader *self, PyObject *args)
{
	return PyString_FromString( self->ss->getName().c_str() );
}

PyObject *StrokeShader_shade( BPy_StrokeShader *self , PyObject *args) {
	PyObject *py_s = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_s) && BPy_Stroke_Check(py_s) )) {
		cout << "ERROR: StrokeShader_shade" << endl;
		Py_RETURN_NONE;
	}
	
	self->ss->shade(*( ((BPy_Stroke *) py_s)->s ));

	Py_RETURN_NONE;
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
