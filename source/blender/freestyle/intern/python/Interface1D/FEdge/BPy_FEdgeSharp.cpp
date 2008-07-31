#include "BPy_FEdgeSharp.h"

#include "../../BPy_Convert.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for FEdgeSharp instance  -----------*/
static int FEdgeSharp___init__(BPy_FEdgeSharp *self, PyObject *args, PyObject *kwds);

static PyObject * FEdgeSharp_normalA( BPy_FEdgeSharp *self ) ;
static PyObject * FEdgeSharp_normalB( BPy_FEdgeSharp *self );
static PyObject * FEdgeSharp_aMaterialIndex( BPy_FEdgeSharp *self ) ;
static PyObject * FEdgeSharp_bMaterialIndex( BPy_FEdgeSharp *self );
static PyObject * FEdgeSharp_aMaterial( BPy_FEdgeSharp *self );
static PyObject * FEdgeSharp_bMaterial( BPy_FEdgeSharp *self );
static PyObject * FEdgeSharp_setNormalA( BPy_FEdgeSharp *self, PyObject *args );
static PyObject * FEdgeSharp_setNormalB( BPy_FEdgeSharp *self, PyObject *args );
static PyObject * FEdgeSharp_setaMaterialIndex( BPy_FEdgeSharp *self, PyObject *args );
static PyObject * FEdgeSharp_setbMaterialIndex( BPy_FEdgeSharp *self, PyObject *args );

/*----------------------FEdgeSharp instance definitions ----------------------------*/
static PyMethodDef BPy_FEdgeSharp_methods[] = {	
	{"normalA", ( PyCFunction ) FEdgeSharp_normalA, METH_NOARGS, "() Returns the normal to the face lying on the right of the FEdge. If this FEdge is a border, it has no Face on its right and therefore, no normal."},
	{"normalB", ( PyCFunction ) FEdgeSharp_normalB, METH_NOARGS, "() Returns the normal to the face lying on the left of the FEdge."},
	{"aMaterialIndex", ( PyCFunction ) FEdgeSharp_aMaterialIndex, METH_NOARGS, "() Returns the index of the material of the face lying on the right of the FEdge. If this FEdge is a border, it has no Face on its right and therefore, no material. "},
	{"bMaterialIndex", ( PyCFunction ) FEdgeSharp_bMaterialIndex, METH_NOARGS, "() Returns the material of the face lying on the left of the FEdge. "},
	{"aMaterial", ( PyCFunction ) FEdgeSharp_aMaterial, METH_NOARGS, "() Returns the material of the face lying on the right of the FEdge. If this FEdge is a border, it has no Face on its right and therefore, no material."},
	{"bMaterial", ( PyCFunction ) FEdgeSharp_bMaterial, METH_NOARGS, "() Returns the material of the face lying on the left of the FEdge."},
	{"setNormalA", ( PyCFunction ) FEdgeSharp_setNormalA, METH_VARARGS, "([x,y,z]) Sets the normal to the face lying on the right of the FEdge."},
	{"setNormalB", ( PyCFunction ) FEdgeSharp_setNormalB, METH_VARARGS, "([x,y,z]) Sets the normal to the face lying on the left of the FEdge. "},
	{"setaMaterialIndex", ( PyCFunction ) FEdgeSharp_setaMaterialIndex, METH_VARARGS, "(unsigned int i) Sets the index of the material lying on the right of the FEdge. "},
	{"setbMaterialIndex", ( PyCFunction ) FEdgeSharp_setbMaterialIndex, METH_VARARGS, "(unsigned int i) Sets the index of the material lying on the left of the FEdge. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdgeSharp type definition ------------------------------*/

PyTypeObject FEdgeSharp_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"FEdgeSharp",				/* tp_name */
	sizeof( BPy_FEdgeSharp ),	/* tp_basicsize */
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
	BPy_FEdgeSharp_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&FEdge_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)FEdgeSharp___init__,                       	/* initproc tp_init; */
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

int FEdgeSharp___init__(BPy_FEdgeSharp *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->fes = new FEdgeSharp();
		
	} else if( BPy_FEdgeSharp_Check(obj1) ) {
		if( ((BPy_FEdgeSharp *) obj1)->fes )
			self->fes = new FEdgeSharp(*( ((BPy_FEdgeSharp *) obj1)->fes ));
		else
			return -1;
		
	} else if( BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fes = new FEdgeSharp( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );

	} else {
		return -1;
	}

	self->py_fe.fe = self->fes;
	self->py_fe.py_if1D.if1D = self->fes;

	return 0;
}


PyObject * FEdgeSharp_normalA( BPy_FEdgeSharp *self ) {
	Vec3r v( self->fes->normalA() );
	return Vector_from_Vec3r( v );
}

PyObject * FEdgeSharp_normalB( BPy_FEdgeSharp *self ) {
	Vec3r v( self->fes->normalB() );
	return Vector_from_Vec3r( v );
}

PyObject * FEdgeSharp_aMaterialIndex( BPy_FEdgeSharp *self ) {
	return PyInt_FromLong( self->fes->aMaterialIndex() );
}

PyObject * FEdgeSharp_bMaterialIndex( BPy_FEdgeSharp *self ) {
	return PyInt_FromLong( self->fes->bMaterialIndex() );
}

PyObject * FEdgeSharp_aMaterial( BPy_FEdgeSharp *self ) {
	Material m( self->fes->aMaterial() );
	return BPy_Material_from_Material(m);
}

PyObject * FEdgeSharp_bMaterial( BPy_FEdgeSharp *self ) {
	Material m( self->fes->aMaterial() );
	return BPy_Material_from_Material(m);
}

PyObject * FEdgeSharp_setNormalA( BPy_FEdgeSharp *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 2 )) {
		cout << "ERROR: FEdgeSharp_setNormalA" << endl;
		Py_RETURN_NONE;
	}
	
	Vec3r v(	PyFloat_AsDouble( PyList_GetItem(obj,0) ),
				PyFloat_AsDouble( PyList_GetItem(obj,1) ),
				PyFloat_AsDouble( PyList_GetItem(obj,2) ) );

	self->fes->setNormalA( v );

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setNormalB( BPy_FEdgeSharp *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 2 )) {
		cout << "ERROR: FEdgeSharp_setNormalB" << endl;
		Py_RETURN_NONE;
	}
	
	Vec3r v(	PyFloat_AsDouble( PyList_GetItem(obj,0) ),
				PyFloat_AsDouble( PyList_GetItem(obj,1) ),
				PyFloat_AsDouble( PyList_GetItem(obj,2) ) );

	self->fes->setNormalB( v );

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setaMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) )) {
		cout << "ERROR: FEdgeSharp_setaMaterialIndex" << endl;
		Py_RETURN_NONE;
	}
	
	self->fes->setaMaterialIndex( i );

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setbMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) )) {
		cout << "ERROR: FEdgeSharp_setbMaterialIndex" << endl;
		Py_RETURN_NONE;
	}
	
	self->fes->setbMaterialIndex( i );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
