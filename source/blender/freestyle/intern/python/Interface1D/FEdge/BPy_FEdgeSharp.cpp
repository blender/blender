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
	PyObject_HEAD_INIT(NULL)
	"FEdgeSharp",                   /* tp_name */
	sizeof(BPy_FEdgeSharp),         /* tp_basicsize */
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
	"FEdgeSharp objects",           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FEdgeSharp_methods,         /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&FEdge_Type,                    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FEdgeSharp___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int FEdgeSharp___init__(BPy_FEdgeSharp *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->fes = new FEdgeSharp();
		
	} else if( !obj2 && BPy_FEdgeSharp_Check(obj1) ) {
		self->fes = new FEdgeSharp(*( ((BPy_FEdgeSharp *) obj1)->fes ));
		
	} else if( obj2 && BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fes = new FEdgeSharp( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_fe.fe = self->fes;
	self->py_fe.py_if1D.if1D = self->fes;
	self->py_fe.py_if1D.borrowed = 0;

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
	return PyLong_FromLong( self->fes->aFrsMaterialIndex() );
}

PyObject * FEdgeSharp_bMaterialIndex( BPy_FEdgeSharp *self ) {
	return PyLong_FromLong( self->fes->bFrsMaterialIndex() );
}

PyObject * FEdgeSharp_aMaterial( BPy_FEdgeSharp *self ) {
	FrsMaterial m( self->fes->aFrsMaterial() );
	return BPy_FrsMaterial_from_FrsMaterial(m);
}

PyObject * FEdgeSharp_bMaterial( BPy_FEdgeSharp *self ) {
	FrsMaterial m( self->fes->aFrsMaterial() );
	return BPy_FrsMaterial_from_FrsMaterial(m);
}

PyObject * FEdgeSharp_setNormalA( BPy_FEdgeSharp *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;
	Vec3r *v = Vec3r_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->fes->setNormalA( *v );
	delete v;

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setNormalB( BPy_FEdgeSharp *self, PyObject *args ) {
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;
	Vec3r *v = Vec3r_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->fes->setNormalB( *v );
	delete v;

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setaMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;
	
	self->fes->setaFrsMaterialIndex( i );

	Py_RETURN_NONE;
}

PyObject * FEdgeSharp_setbMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;
	
	self->fes->setbFrsMaterialIndex( i );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
