#include "Id.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Id instance  -----------*/
static PyObject * Id___new__(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Id___dealloc__(BPy_Id *self);
static int Id___init__(BPy_Id *self, PyObject *args, PyObject *kwds);
static PyObject * Id___repr__(BPy_Id* self);

static PyObject * Id_getFirst( BPy_Id *self );
static PyObject * Id_getSecond( BPy_Id *self);
static PyObject * Id_setFirst( BPy_Id *self , PyObject *args);
static PyObject * Id_setSecond( BPy_Id *self , PyObject *args);
static PyObject * Id___eq__( BPy_Id *self , PyObject *args);
static PyObject * Id___ne__( BPy_Id *self , PyObject *args);
static PyObject * Id___lt__( BPy_Id *self , PyObject *args);

/*----------------------Id instance definitions ----------------------------*/
static PyMethodDef BPy_Id_methods[] = {
	{"getFirst", ( PyCFunction ) Id_getFirst, METH_NOARGS, "Returns the first Id number"},
	{"getSecond", ( PyCFunction ) Id_getSecond, METH_NOARGS, "Returns the second Id number" },
	{"setFirst", ( PyCFunction ) Id_setFirst, METH_VARARGS, "Sets the first number constituing the Id" },
	{"setSecond", ( PyCFunction ) Id_setSecond, METH_VARARGS, "Sets the second number constituing the Id" },
	{"__eq__", ( PyCFunction ) Id___eq__, METH_VARARGS, "Operator ==" },
	{"__ne__", ( PyCFunction ) Id___ne__, METH_VARARGS, "Operator !=" },
	{"__lt__", ( PyCFunction ) Id___lt__, METH_VARARGS, "Operator <" },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Id type definition ------------------------------*/

PyTypeObject Id_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Id",						/* tp_name */
	sizeof( BPy_Id ),			/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Id___dealloc__,	/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	(reprfunc)Id___repr__,				/* tp_repr */

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
	Py_TPFLAGS_DEFAULT, 		/* long tp_flags; */

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
	BPy_Id_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Id___init__,           /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	Id___new__,						/* newfunc tp_new; */
	
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
PyMODINIT_FUNC Id_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &Id_Type ) < 0 )
		return;

	Py_INCREF( &Id_Type );
	PyModule_AddObject(module, "Id", (PyObject *)&Id_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

PyObject * Id___new__(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BPy_Id *self;

    self = (BPy_Id *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->id = new Id();
    }

    return (PyObject *)self;
}

void Id___dealloc__(BPy_Id* self)
{
	delete self->id;
    self->ob_type->tp_free((PyObject*)self);
}

int Id___init__(BPy_Id *self, PyObject *args, PyObject *kwds)
{
    int first = 0, second = 0;
    static char *kwlist[] = {"first", "second", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &first, &second) )
        return -1;

	self->id->setFirst( first );
	self->id->setSecond( second );

    return 0;
}

PyObject * Id___repr__(BPy_Id* self)
{
    return PyString_FromFormat("[ first: %i, second: %i ](BPy_Id)", self->id->getFirst(), self->id->getSecond() );
}

PyObject *Id_getFirst( BPy_Id *self ) {
	return PyInt_FromLong( self->id->getFirst() );
}


PyObject *Id_getSecond( BPy_Id *self) {
	return PyInt_FromLong( self->id->getSecond() );
}


PyObject *Id_setFirst( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, (char *)"i:Id_setFirst", i) ) {
		cout << "ERROR: Id_setFirst" << endl;
		Py_RETURN_NONE;
	}

	self->id->setFirst( i );

	Py_RETURN_NONE;
}


PyObject *Id_setSecond( BPy_Id *self , PyObject *args) {
	unsigned int i;

	if( !PyArg_ParseTuple(args, (char *)"i:Id_setSecond", i) ) {
		cout << "ERROR: Id_setSecond" << endl;
		Py_RETURN_NONE;
	}

	self->id->setSecond( i );

	Py_RETURN_NONE;
}

PyObject *Id___eq__( BPy_Id *self , PyObject *args) {
	BPy_Id * other = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"O:Id___eq__", &other) ) {
		cout << "ERROR: Id___eq__" << endl;
		Py_RETURN_NONE;
	}
	
	return PyBool_from_bool( self->id == other->id );
}


PyObject *Id___ne__(BPy_Id *self , PyObject *args) {
	BPy_Id * other = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"O:Id___ne__", &other) ) {
		cout << "ERROR: Id___ne__" << endl;
		Py_RETURN_NONE;
	}
	
	return PyBool_from_bool( self->id != other->id );
}

PyObject *Id___lt__(BPy_Id *self , PyObject *args) {
	BPy_Id * other = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"O:Id___lt__", &other) ) {
		cout << "ERROR: Id___lt__" << endl;
		Py_RETURN_NONE;
	}
	
	return PyBool_from_bool( self->id <= other->id );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif