#include "BPy_FEdgeSmooth.h"

#include "../../BPy_Convert.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for FEdgeSmooth instance  -----------*/
static int FEdgeSmooth___init__(BPy_FEdgeSmooth *self, PyObject *args, PyObject *kwds);

static PyObject * FEdgeSmooth_normal( BPy_FEdgeSmooth *self ) ;
static PyObject * FEdgeSmooth_materialIndex( BPy_FEdgeSmooth *self ) ;
static PyObject * FEdgeSmooth_material( BPy_FEdgeSmooth *self );
static PyObject * FEdgeSmooth_setNormal( BPy_FEdgeSmooth *self, PyObject *args );
static PyObject * FEdgeSmooth_setMaterialIndex( BPy_FEdgeSmooth *self, PyObject *args );


/*----------------------FEdgeSmooth instance definitions ----------------------------*/
static PyMethodDef BPy_FEdgeSmooth_methods[] = {	
	{"normal", ( PyCFunction ) FEdgeSmooth_normal, METH_NOARGS, "() Returns the normal to the Face it is running accross."},
	{"materialIndex", ( PyCFunction ) FEdgeSmooth_materialIndex, METH_NOARGS, "() Returns the index of the material of the face it is running accross. "},
	{"aMaterial", ( PyCFunction ) FEdgeSmooth_material, METH_NOARGS, "() Returns the material of the face it is running accross. "},
	{"setNormalA", ( PyCFunction ) FEdgeSmooth_setNormal, METH_VARARGS, "([x,y,z]) Sets the normal to the Face it is running accross."},
	{"setaMaterialIndex", ( PyCFunction ) FEdgeSmooth_setMaterialIndex, METH_VARARGS, "(unsigned int i) Sets the index of the material of the face it is running accross. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdgeSmooth type definition ------------------------------*/

PyTypeObject FEdgeSmooth_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"FEdgeSmooth",				/* tp_name */
	sizeof( BPy_FEdgeSmooth ),	/* tp_basicsize */
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
	BPy_FEdgeSmooth_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&FEdge_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)FEdgeSmooth___init__,                       	/* initproc tp_init; */
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


int FEdgeSmooth___init__(BPy_FEdgeSmooth *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->fes = new FEdgeSmooth();
		
	} else if( BPy_FEdgeSmooth_Check(obj1) ) {
		if( ((BPy_FEdgeSmooth *) obj1)->fes )
			self->fes = new FEdgeSmooth(*( ((BPy_FEdgeSmooth *) obj1)->fes ));
		else
			return -1;
		
	} else if( BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fes = new FEdgeSmooth( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );

	} else {
		return -1;
	}

	self->py_fe.fe = self->fes;
	self->py_fe.py_if1D.if1D = self->fes;

	return 0;
}

PyObject * FEdgeSmooth_normal( BPy_FEdgeSmooth *self ) {
	Vec3r v( self->fes->normal() );
	return Vector_from_Vec3r( v );
}

PyObject * FEdgeSmooth_materialIndex( BPy_FEdgeSmooth *self ) {
	return PyInt_FromLong( self->fes->materialIndex() );
}


PyObject * FEdgeSmooth_material( BPy_FEdgeSmooth *self ) {
	Material m( self->fes->material() );
	return BPy_FrsMaterial_from_Material(m);
}

PyObject * FEdgeSmooth_setNormal( BPy_FEdgeSmooth *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 2 )) {
		cout << "ERROR: FEdgeSmooth_setNormal" << endl;
		Py_RETURN_NONE;
	}
	
	Vec3r v(	PyFloat_AsDouble( PyList_GetItem(obj,0) ),
				PyFloat_AsDouble( PyList_GetItem(obj,1) ),
				PyFloat_AsDouble( PyList_GetItem(obj,2) ) );

	self->fes->setNormal( v );

	Py_RETURN_NONE;
}

PyObject * FEdgeSmooth_setMaterialIndex( BPy_FEdgeSmooth *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) )) {
		cout << "ERROR: FEdgeSmooth_setMaterialIndex" << endl;
		Py_RETURN_NONE;
	}
	
	self->fes->setMaterialIndex( i );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
