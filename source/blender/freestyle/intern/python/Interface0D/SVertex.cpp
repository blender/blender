#include "SVertex.h"

#include "../Convert.h"
#include "../Id.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for SVertex instance  -----------*/
static int SVertex___init__(BPy_SVertex *self, PyObject *args, PyObject *kwds);
static PyObject * SVertex___copy__( BPy_SVertex *self );
static PyObject * SVertex_normals( BPy_SVertex *self );
static PyObject * SVertex_normalsSize( BPy_SVertex *self );
static PyObject * SVertex_SetPoint3D( BPy_SVertex *self , PyObject *args);
static PyObject * SVertex_SetPoint2D( BPy_SVertex *self , PyObject *args);
static PyObject * SVertex_AddNormal( BPy_SVertex *self , PyObject *args);
static PyObject * SVertex_SetId( BPy_SVertex *self , PyObject *args);
/*----------------------SVertex instance definitions ----------------------------*/
static PyMethodDef BPy_SVertex_methods[] = {
	{"__copy__", ( PyCFunction ) SVertex___copy__, METH_NOARGS, "（ ）Cloning method."},
	{"normals", ( PyCFunction ) SVertex_normals, METH_NOARGS, "Returns the normals for this Vertex as a list. In a smooth surface, a vertex has exactly one normal. In a sharp surface, a vertex can have any number of normals."},
	{"normalsSize", ( PyCFunction ) SVertex_normalsSize, METH_NOARGS, "Returns the number of different normals for this vertex." },
	{"SetPoint3D", ( PyCFunction ) SVertex_SetPoint3D, METH_VARARGS, "Sets the 3D coordinates of the SVertex." },
	{"SetPoint2D", ( PyCFunction ) SVertex_SetPoint2D, METH_VARARGS, "Sets the 3D projected coordinates of the SVertex." },
	{"AddNormal", ( PyCFunction ) SVertex_AddNormal, METH_VARARGS, "Adds a normal to the Svertex's set of normals. If the same normal is already in the set, nothing changes." },
	{"SetId", ( PyCFunction ) SVertex_SetId, METH_VARARGS, "Sets the Id." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_SVertex type definition ------------------------------*/

PyTypeObject SVertex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"SVertex",				/* tp_name */
	sizeof( BPy_SVertex ),	/* tp_basicsize */
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
	BPy_SVertex_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface0D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)SVertex___init__,                       	/* initproc tp_init; */
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

int SVertex___init__(BPy_SVertex *self, PyObject *args, PyObject *kwds)
{
	PyObject *py_point = 0;
	BPy_Id *py_id = 0;

    if (! PyArg_ParseTuple(args, "|OO", &py_point, &py_id) )
        return -1;
	
	if( py_point && py_id && PyList_Check(py_point) && PyList_Size(py_point) == 3 ) {
		Vec3r v( 	PyFloat_AsDouble( PyList_GetItem(py_point, 0) ),
					PyFloat_AsDouble( PyList_GetItem(py_point, 1) ),
					PyFloat_AsDouble( PyList_GetItem(py_point, 2) )  );
					
		self->sv = new SVertex( v, *(py_id->id) );
	} else {
		self->sv = new SVertex();
	}
	
	self->py_if0D.if0D = self->sv;
	
	return 0;
}

PyObject * SVertex___copy__( BPy_SVertex *self ) {
	BPy_SVertex *py_svertex;
	
	py_svertex = (BPy_SVertex *) SVertex_Type.tp_new( &SVertex_Type, 0, 0 );
	
	py_svertex->sv = self->sv->duplicate();
	py_svertex->py_if0D.if0D = py_svertex->sv;

	return (PyObject *) py_svertex;
}


PyObject * SVertex_normals( BPy_SVertex *self ) {
	PyObject *py_normals; 
	set< Vec3r > normals;
	
	py_normals  = PyList_New(NULL);
	normals = self->sv->normals();
		
	for( set< Vec3r >::iterator set_iterator = normals.begin(); set_iterator != normals.end(); set_iterator++ ) {
		PyList_Append( py_normals, Vector_from_Vec3r(*set_iterator) );
	}
	
	return py_normals;
}

PyObject * SVertex_normalsSize( BPy_SVertex *self ) {
	return PyInt_FromLong( self->sv->normalsSize() );
}

PyObject *SVertex_SetPoint3D( BPy_SVertex *self , PyObject *args) {
	PyObject *py_point;

	if(!( PyArg_ParseTuple(args, "O", &py_point) 
			&& PyList_Check(py_point) && PyList_Size(py_point) == 3 )) {
		cout << "ERROR: SVertex_SetPoint3D" << endl;
		Py_RETURN_NONE;
	}

	Vec3r v( 	PyFloat_AsDouble( PyList_GetItem(py_point, 0) ),
				PyFloat_AsDouble( PyList_GetItem(py_point, 1) ),
				PyFloat_AsDouble( PyList_GetItem(py_point, 2) )  );
	self->sv->SetPoint3D( v );

	Py_RETURN_NONE;
}

PyObject *SVertex_SetPoint2D( BPy_SVertex *self , PyObject *args) {
	PyObject *py_point;

	if(!( PyArg_ParseTuple(args, "O", &py_point) 
			&& PyList_Check(py_point) && PyList_Size(py_point) == 3 )) {
		cout << "ERROR: SVertex_SetPoint2D" << endl;
		Py_RETURN_NONE;
	}

	Vec3r v( 	PyFloat_AsDouble( PyList_GetItem(py_point, 0) ),
				PyFloat_AsDouble( PyList_GetItem(py_point, 1) ),
				PyFloat_AsDouble( PyList_GetItem(py_point, 2) )  );
	self->sv->SetPoint2D( v );

	Py_RETURN_NONE;
}

PyObject *SVertex_AddNormal( BPy_SVertex *self , PyObject *args) {
	PyObject *py_normal;

	if(!( PyArg_ParseTuple(args, "O", &py_normal) 
			&& PyList_Check(py_normal) && PyList_Size(py_normal) == 3 )) {
		cout << "ERROR: SVertex_AddNormal" << endl;
		Py_RETURN_NONE;
	}
	
	cout << "yoyo" << endl;

	Vec3r n( 	PyFloat_AsDouble( PyList_GetItem(py_normal, 0) ),
				PyFloat_AsDouble( PyList_GetItem(py_normal, 1) ),
				PyFloat_AsDouble( PyList_GetItem(py_normal, 2) )  );
	self->sv->AddNormal( n );

	Py_RETURN_NONE;
}

PyObject *SVertex_SetId( BPy_SVertex *self , PyObject *args) {
	BPy_Id *py_id;

	if( !PyArg_ParseTuple(args, "O", &py_id) ) {
		cout << "ERROR: SVertex_SetId" << endl;
		Py_RETURN_NONE;
	}

	self->sv->SetId( *(py_id->id) );

	Py_RETURN_NONE;
}

// virtual bool 	operator== (const SVertex &iBrother)
// ViewVertex * 	viewvertex ()
// void 	AddFEdge (FEdge *iFEdge)

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

