#include "BPy_ViewEdgeIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewEdgeIterator instance  -----------*/
static int ViewEdgeIterator___init__(BPy_ViewEdgeIterator *self, PyObject *args);

static PyObject * ViewEdgeIterator_getCurrentEdge( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setCurrentEdge( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_getBegin( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setBegin( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_getOrientation( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setOrientation( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_changeOrientation( BPy_ViewEdgeIterator *self );

static PyObject * ViewEdgeIterator_getObject(BPy_ViewEdgeIterator *self);


/*----------------------ViewEdgeIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdgeIterator_methods[] = {
	{"getCurrentEdge", ( PyCFunction ) ViewEdgeIterator_getCurrentEdge, METH_NOARGS, "() Returns the current pointed ViewEdge."},
	{"setCurrentEdge", ( PyCFunction ) ViewEdgeIterator_setCurrentEdge, METH_VARARGS, "(ViewEdge ve) Sets the current pointed ViewEdge. "},	
	{"getBegin", ( PyCFunction ) ViewEdgeIterator_getBegin, METH_NOARGS, "() Returns the first ViewEdge used for the iteration."},
	{"setBegin", ( PyCFunction ) ViewEdgeIterator_setBegin, METH_VARARGS, "(ViewEdge ve) Sets the first ViewEdge used for the iteration."},
	{"getOrientation", ( PyCFunction ) ViewEdgeIterator_getOrientation, METH_NOARGS, "() Gets the orientation of the pointed ViewEdge in the iteration. "},
	{"setOrientation", ( PyCFunction ) ViewEdgeIterator_setOrientation, METH_VARARGS, "(bool b) Sets the orientation of the pointed ViewEdge in the iteration. "},
	{"changeOrientation", ( PyCFunction ) ViewEdgeIterator_changeOrientation, METH_NOARGS, "() Changes the current orientation."},
	{"getObject", ( PyCFunction ) ViewEdgeIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewEdgeIterator type definition ------------------------------*/

PyTypeObject ViewEdgeIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ViewEdgeIterator",				/* tp_name */
	sizeof( BPy_ViewEdgeIterator ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	NULL,					/* tp_repr */

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
	BPy_ViewEdgeIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Iterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ViewEdgeIterator___init__,                       	/* initproc tp_init; */
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

//------------------------INSTANCE METHODS ----------------------------------

int ViewEdgeIterator___init__(BPy_ViewEdgeIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0;

	if (!( PyArg_ParseTuple(args, "O|O", &obj1, &obj2) ))
	    return -1;

	if( obj1 && BPy_ViewEdgeIterator_Check(obj1)  ) {
		self->ve_it = new ViewEdgeInternal::ViewEdgeIterator(*( ((BPy_ViewEdgeIterator *) obj1)->ve_it ));
	
	} else {
		ViewEdge *begin = ( obj1 && BPy_ViewEdge_Check(obj1) ) ? ((BPy_ViewEdge *) obj1)->ve : 0;
		bool orientation = ( obj2 && PyBool_Check(obj2) ) ? bool_from_PyBool(obj2) : true;
		
		self->ve_it = new ViewEdgeInternal::ViewEdgeIterator( begin, orientation);
		
	}
		
	self->py_it.it = self->ve_it;
	
	return 0;
}


PyObject *ViewEdgeIterator_getCurrentEdge( BPy_ViewEdgeIterator *self ) {
	if( self->ve_it->getCurrentEdge() )
		return BPy_ViewEdge_from_ViewEdge(*( self->ve_it->getCurrentEdge()  ));
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_setCurrentEdge( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O", &py_ve) && BPy_ViewEdge_Check(py_ve) )) {
		cout << "ERROR: ViewEdgeIterator_setCurrentEdge" << endl;
		Py_RETURN_NONE;
	}

	self->ve_it->setCurrentEdge( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}


PyObject *ViewEdgeIterator_getBegin( BPy_ViewEdgeIterator *self ) {
	if( self->ve_it->getBegin() )
		return BPy_ViewEdge_from_ViewEdge(*( self->ve_it->getBegin()  ));
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_setBegin( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O", &py_ve) && BPy_ViewEdge_Check(py_ve) )) {
		cout << "ERROR: ViewEdgeIterator_setBegin" << endl;
		Py_RETURN_NONE;
	}

	self->ve_it->setBegin( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_getOrientation( BPy_ViewEdgeIterator *self ) {
	return PyBool_from_bool( self->ve_it->getOrientation() );
}

PyObject *ViewEdgeIterator_setOrientation( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) && PyBool_Check(py_b) )) {
		cout << "ERROR: ViewEdgeIterator_setOrientation" << endl;
		Py_RETURN_NONE;
	}

	self->ve_it->setOrientation( bool_from_PyBool(py_b) );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_changeOrientation( BPy_ViewEdgeIterator *self ) {
	self->ve_it->changeOrientation();
	
	Py_RETURN_NONE;
}

PyObject * ViewEdgeIterator_getObject( BPy_ViewEdgeIterator *self) {

	ViewEdge *ve = self->ve_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
