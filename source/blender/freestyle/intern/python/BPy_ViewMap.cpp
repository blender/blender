#include "BPy_ViewMap.h"

#include "BPy_Convert.h"
#include "BPy_BBox.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewMap instance  -----------*/
static int ViewMap___init__(BPy_ViewMap *self, PyObject *args, PyObject *kwds);
static void ViewMap___dealloc__(BPy_ViewMap *self);
static PyObject * ViewMap___repr__(BPy_ViewMap* self);

static PyObject * ViewMap_getClosestViewEdge( BPy_ViewMap *self , PyObject *args);
static PyObject * ViewMap_getClosestFEdge( BPy_ViewMap *self , PyObject *args);
static PyObject * ViewMap_getScene3dBBox( BPy_ViewMap *self , PyObject *args);
static PyObject * ViewMap_setScene3dBBox( BPy_ViewMap *self , PyObject *args);

/*---------------------- BPy_ViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_ViewMap_methods[] = {
	{"getClosestViewEdge", ( PyCFunction ) ViewMap_getClosestViewEdge, METH_VARARGS, "(double x, double y) Gets the viewedge the nearest to the 2D position specified as argument "},	
	{"getClosestFEdge", ( PyCFunction ) ViewMap_getClosestFEdge, METH_VARARGS, "(double x, double y) Gets the Fedge the nearest to the 2D position specified as argument "},
	{"getScene3dBBox", ( PyCFunction ) ViewMap_getScene3dBBox, METH_NOARGS, "() Returns the scene 3D bounding box. "},
	{"setScene3dBBox", ( PyCFunction ) ViewMap_setScene3dBBox, METH_VARARGS, "(BBox bb) Sets the scene 3D bounding box."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewMap type definition ------------------------------*/

PyTypeObject ViewMap_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ViewMap",						/* tp_name */
	sizeof( BPy_ViewMap ),			/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)ViewMap___dealloc__,	/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	(reprfunc)ViewMap___repr__,				/* tp_repr */

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
	NULL,                   /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ViewMap_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ViewMap___init__,           /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,						/* newfunc tp_new; */
	
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
PyMODINIT_FUNC ViewMap_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &ViewMap_Type ) < 0 )
		return;

	Py_INCREF( &ViewMap_Type );
	PyModule_AddObject(module, "ViewMap", (PyObject *)&ViewMap_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int ViewMap___init__(BPy_ViewMap *self, PyObject *args, PyObject *kwds)
{
	self->vm = new ViewMap();
    return 0;
}

void ViewMap___dealloc__(BPy_ViewMap *self)
{
	delete self->vm;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * ViewMap___repr__(BPy_ViewMap *self)
{
    return PyString_FromFormat("ViewMap - address: %p", self->vm );
}

PyObject * ViewMap_getClosestViewEdge( BPy_ViewMap *self , PyObject *args) {
	double x, y;

	if(!( PyArg_ParseTuple(args, "dd", &x, &y) )) {
		cout << "ERROR: ViewMap_getClosestFEdge" << endl;
		Py_RETURN_NONE;
	}

	ViewEdge *ve = const_cast<ViewEdge *>( self->vm->getClosestViewEdge(x,y) );
	if( ve )
		return BPy_ViewEdge_from_ViewEdge(*ve);

	Py_RETURN_NONE;
}

PyObject * ViewMap_getClosestFEdge( BPy_ViewMap *self , PyObject *args) {
	double x, y;

	if(!( PyArg_ParseTuple(args, "dd", &x, &y) )) {
		cout << "ERROR: ViewMap_getClosestFEdge" << endl;
		Py_RETURN_NONE;
	}

	FEdge *fe = const_cast<FEdge *>( self->vm->getClosestFEdge(x,y) );
	if( fe )
		return BPy_FEdge_from_FEdge(*fe);

	Py_RETURN_NONE;
}

PyObject * ViewMap_getScene3dBBox( BPy_ViewMap *self , PyObject *args) {
	BBox<Vec3r> bb( self->vm->getScene3dBBox() );
	return BPy_BBox_from_BBox( bb );
}

PyObject * ViewMap_setScene3dBBox( BPy_ViewMap *self , PyObject *args) {
	PyObject *py_bb = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_bb) && BPy_BBox_Check(py_bb) )) {
		cout << "ERROR: ViewMap_setScene3dBBox" << endl;
		Py_RETURN_NONE;
	}

	self->vm->setScene3dBBox(*( ((BPy_BBox *) py_bb)->bb ));

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif