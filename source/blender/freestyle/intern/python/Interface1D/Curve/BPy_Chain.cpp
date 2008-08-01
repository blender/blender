#include "BPy_Chain.h"

#include "../../BPy_Convert.h"
#include "../../BPy_Id.h"
#include "../BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Chain instance  -----------*/
static int Chain___init__(BPy_Chain *self, PyObject *args, PyObject *kwds);
static PyObject * Chain_push_viewedge_back( BPy_Chain *self, PyObject *args );
static PyObject * Chain_push_viewedge_front( BPy_Chain *self, PyObject *args );


/*----------------------Chain instance definitions ----------------------------*/
static PyMethodDef BPy_Chain_methods[] = {	
	{"push_viewedge_back", ( PyCFunction ) Chain_push_viewedge_back, METH_VARARGS, "(ViewEdge ve, bool orientation) Adds a ViewEdge at the end of the chain."},
	{"push_viewedge_front", ( PyCFunction ) Chain_push_viewedge_front, METH_VARARGS, "(ViewEdge ve, bool orientation) Adds a ViewEdge at the beginning of the chain."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Chain type definition ------------------------------*/

PyTypeObject Chain_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Chain",				/* tp_name */
	sizeof( BPy_Chain ),	/* tp_basicsize */
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
	BPy_Chain_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&FrsCurve_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Chain___init__,                       	/* initproc tp_init; */
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


//------------------------INSTANCE METHODS ----------------------------------

int Chain___init__(BPy_Chain *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj = 0;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ){
		self->c = new Chain();
		
	} else if( BPy_Chain_Check(obj) ) {
		if( ((BPy_Chain *) obj)->c )
			self->c = new Chain(*( ((BPy_Chain *) obj)->c ));
		else
			return -1;
		
	} else if( BPy_Id_Check(obj) ) {
		if( ((BPy_Id *) obj)->id )
			self->c = new Chain(*( ((BPy_Id *) obj)->id ));
		else
			return -1;
			
	} else {
		return -1;
	}

	self->py_c.c = self->c;
	self->py_c.py_if1D.if1D = self->c;

	return 0;
}


PyObject * Chain_push_viewedge_back( BPy_Chain *self, PyObject *args ) {
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( PyArg_ParseTuple(args, "OO", &obj1, &obj2) &&	BPy_ViewEdge_Check(obj1) && PyBool_Check(obj2) )) {
		cout << "ERROR: Chain_push_viewedge_back" << endl;
		Py_RETURN_NONE;
	}

	ViewEdge *ve = ((BPy_ViewEdge *) obj1)->ve;
	bool orientation = bool_from_PyBool( obj2 );
	self->c->push_viewedge_back( ve, orientation);

	Py_RETURN_NONE;
}

PyObject * Chain_push_viewedge_front( BPy_Chain *self, PyObject *args ) {
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( PyArg_ParseTuple(args, "OO", &obj1, &obj2) &&	BPy_ViewEdge_Check(obj1) && PyBool_Check(obj2) )) {
		cout << "ERROR: Chain_push_viewedge_front" << endl;
		Py_RETURN_NONE;
	}

	ViewEdge *ve = ((BPy_ViewEdge *) obj1)->ve;
	bool orientation = bool_from_PyBool( obj2 );
	self->c->push_viewedge_front(ve, orientation);

	Py_RETURN_NONE;
}





///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
