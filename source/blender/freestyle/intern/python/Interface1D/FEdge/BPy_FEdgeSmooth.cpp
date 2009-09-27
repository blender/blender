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
	{"material", ( PyCFunction ) FEdgeSmooth_material, METH_NOARGS, "() Returns the material of the face it is running accross. "},
	{"setNormal", ( PyCFunction ) FEdgeSmooth_setNormal, METH_VARARGS, "([x,y,z]) Sets the normal to the Face it is running accross."},
	{"setMaterialIndex", ( PyCFunction ) FEdgeSmooth_setMaterialIndex, METH_VARARGS, "(unsigned int i) Sets the index of the material of the face it is running accross. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdgeSmooth type definition ------------------------------*/

PyTypeObject FEdgeSmooth_Type = {
	PyObject_HEAD_INIT(NULL)
	"FEdgeSmooth",                  /* tp_name */
	sizeof(BPy_FEdgeSmooth),        /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"FEdgeSmooth objects",          /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FEdgeSmooth_methods,        /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&FEdge_Type,                    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FEdgeSmooth___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------


int FEdgeSmooth___init__(BPy_FEdgeSmooth *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->fes = new FEdgeSmooth();
		
	} else if( !obj2 && BPy_FEdgeSmooth_Check(obj1) ) {
		self->fes = new FEdgeSmooth(*( ((BPy_FEdgeSmooth *) obj1)->fes ));
		
	} else if( obj2 && BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fes = new FEdgeSmooth( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_fe.fe = self->fes;
	self->py_fe.py_if1D.if1D = self->fes;
	self->py_fe.py_if1D.borrowed = 0;

	return 0;
}

PyObject * FEdgeSmooth_normal( BPy_FEdgeSmooth *self ) {
	Vec3r v( self->fes->normal() );
	return Vector_from_Vec3r( v );
}

PyObject * FEdgeSmooth_materialIndex( BPy_FEdgeSmooth *self ) {
	return PyLong_FromLong( self->fes->frs_materialIndex() );
}


PyObject * FEdgeSmooth_material( BPy_FEdgeSmooth *self ) {
	FrsMaterial m( self->fes->frs_material() );
	return BPy_FrsMaterial_from_FrsMaterial(m);
}

PyObject * FEdgeSmooth_setNormal( BPy_FEdgeSmooth *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;
	Vec3r *v = Vec3r_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->fes->setNormal( *v );
	delete v;

	Py_RETURN_NONE;
}

PyObject * FEdgeSmooth_setMaterialIndex( BPy_FEdgeSmooth *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;
	
	self->fes->setFrsMaterialIndex( i );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
