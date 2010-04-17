#include "BPy_FEdgeSharp.h"

#include "../../BPy_Convert.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char FEdgeSharp___doc__[] =
"Class defining a sharp FEdge.  A Sharp FEdge corresponds to an initial\n"
"edge of the input mesh.  It can be a silhouette, a crease or a border.\n"
"If it is a crease edge, then it is borded by two faces of the mesh.\n"
"Face a lies on its right whereas Face b lies on its left.  If it is a\n"
"border edge, then it doesn't have any face on its right, and thus Face\n"
"a is None.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: An FEdgeSharp object.\n"
"   :type iBrother: :class:`FEdgeSharp`\n"
"\n"
".. method:: __init__(vA, vB)\n"
"\n"
"   Builds an FEdgeSharp going from vA to vB.\n"
"\n"
"   :arg vA: The first SVertex object.\n"
"   :type vA: :class:`SVertex`\n"
"   :arg vB: The second SVertex object.\n"
"   :type vB: :class:`SVertex`\n";

static int FEdgeSharp___init__(BPy_FEdgeSharp *self, PyObject *args, PyObject *kwds)
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

static char FEdgeSharp_normalA___doc__[] =
".. method:: normalA()\n"
"\n"
"   Returns the normal to the face lying on the right of the FEdge.  If\n"
"   this FEdge is a border, it has no Face on its right and therefore, no\n"
"   normal.\n"
"\n"
"   :return: The normal to the face lying on the right of the FEdge.\n"
"   :rtype: :class:`Mathutils.Vector`\n";

static PyObject * FEdgeSharp_normalA( BPy_FEdgeSharp *self ) {
	Vec3r v( self->fes->normalA() );
	return Vector_from_Vec3r( v );
}

static char FEdgeSharp_normalB___doc__[] =
".. method:: normalB()\n"
"\n"
"   Returns the normal to the face lying on the left of the FEdge.\n"
"\n"
"   :return: The normal to the face lying on the left of the FEdge.\n"
"   :rtype: :class:`Mathutils.Vector`\n";

static PyObject * FEdgeSharp_normalB( BPy_FEdgeSharp *self ) {
	Vec3r v( self->fes->normalB() );
	return Vector_from_Vec3r( v );
}

static char FEdgeSharp_aMaterialIndex___doc__[] =
".. method:: aMaterialIndex()\n"
"\n"
"   Returns the index of the material of the face lying on the right of\n"
"   the FEdge. If this FEdge is a border, it has no Face on its right and\n"
"   therefore, no material.\n"
"\n"
"   :return: The index of the material of the face lying on the right of\n"
"      the FEdge.\n"
"   :rtype: int\n";

static PyObject * FEdgeSharp_aMaterialIndex( BPy_FEdgeSharp *self ) {
	return PyLong_FromLong( self->fes->aFrsMaterialIndex() );
}

static char FEdgeSharp_bMaterialIndex___doc__[] =
".. method:: bMaterialIndex()\n"
"\n"
"   Returns the index of the material of the face lying on the left of\n"
"   the FEdge.\n"
"\n"
"   :return: The index of the material of the face lying on the left of\n"
"      the FEdge.\n"
"   :rtype: int\n";

static PyObject * FEdgeSharp_bMaterialIndex( BPy_FEdgeSharp *self ) {
	return PyLong_FromLong( self->fes->bFrsMaterialIndex() );
}

static char FEdgeSharp_aMaterial___doc__[] =
".. method:: aMaterial()\n"
"\n"
"   Returns the material of the face lying on the right of the FEdge.  If\n"
"   this FEdge is a border, it has no Face on its right and therefore, no\n"
"   material.\n"
"\n"
"   :return: The material of the face lying on the right of the FEdge.\n"
"   :rtype: :class:`Material`\n";

static PyObject * FEdgeSharp_aMaterial( BPy_FEdgeSharp *self ) {
	FrsMaterial m( self->fes->aFrsMaterial() );
	return BPy_FrsMaterial_from_FrsMaterial(m);
}

static char FEdgeSharp_bMaterial___doc__[] =
".. method:: bMaterial()\n"
"\n"
"   Returns the material of the face lying on the left of the FEdge.\n"
"\n"
"   :return: The material of the face lying on the left of the FEdge.\n"
"   :rtype: :class:`Material`\n";

static PyObject * FEdgeSharp_bMaterial( BPy_FEdgeSharp *self ) {
	FrsMaterial m( self->fes->aFrsMaterial() );
	return BPy_FrsMaterial_from_FrsMaterial(m);
}

static char FEdgeSharp_setNormalA___doc__[] =
".. method:: setNormalA(iNormal)\n"
"\n"
"   Sets the normal to the face lying on the right of the FEdge.\n"
"\n"
"   :arg iNormal: A three-dimensional vector.\n"
"   :type iNormal: :class:`Mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject * FEdgeSharp_setNormalA( BPy_FEdgeSharp *self, PyObject *args ) {
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

static char FEdgeSharp_setNormalB___doc__[] =
".. method:: setNormalB(iNormal)\n"
"\n"
"   Sets the normal to the face lying on the left of the FEdge.\n"
"\n"
"   :arg iNormal: A three-dimensional vector.\n"
"   :type iNormal: :class:`Mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject * FEdgeSharp_setNormalB( BPy_FEdgeSharp *self, PyObject *args ) {
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

static char FEdgeSharp_setaMaterialIndex___doc__[] =
".. method:: setaMaterialIndex(i)\n"
"\n"
"   Sets the index of the material lying on the right of the FEdge.\n"
"\n"
"   :arg i: A material index.\n"
"   :type i: int\n";

static PyObject * FEdgeSharp_setaMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;
	
	self->fes->setaFrsMaterialIndex( i );

	Py_RETURN_NONE;
}

static char FEdgeSharp_setbMaterialIndex___doc__[] =
".. method:: setbMaterialIndex(i)\n"
"\n"
"   Sets the index of the material lying on the left of the FEdge.\n"
"\n"
"   :arg i: A material index.\n"
"   :type i: int\n";

static PyObject * FEdgeSharp_setbMaterialIndex( BPy_FEdgeSharp *self, PyObject *args ) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;
	
	self->fes->setbFrsMaterialIndex( i );

	Py_RETURN_NONE;
}

/*----------------------FEdgeSharp instance definitions ----------------------------*/
static PyMethodDef BPy_FEdgeSharp_methods[] = {	
	{"normalA", ( PyCFunction ) FEdgeSharp_normalA, METH_NOARGS, FEdgeSharp_normalA___doc__},
	{"normalB", ( PyCFunction ) FEdgeSharp_normalB, METH_NOARGS, FEdgeSharp_normalB___doc__},
	{"aMaterialIndex", ( PyCFunction ) FEdgeSharp_aMaterialIndex, METH_NOARGS, FEdgeSharp_aMaterialIndex___doc__},
	{"bMaterialIndex", ( PyCFunction ) FEdgeSharp_bMaterialIndex, METH_NOARGS, FEdgeSharp_bMaterialIndex___doc__},
	{"aMaterial", ( PyCFunction ) FEdgeSharp_aMaterial, METH_NOARGS, FEdgeSharp_aMaterial___doc__},
	{"bMaterial", ( PyCFunction ) FEdgeSharp_bMaterial, METH_NOARGS, FEdgeSharp_bMaterial___doc__},
	{"setNormalA", ( PyCFunction ) FEdgeSharp_setNormalA, METH_VARARGS, FEdgeSharp_setNormalA___doc__},
	{"setNormalB", ( PyCFunction ) FEdgeSharp_setNormalB, METH_VARARGS, FEdgeSharp_setNormalB___doc__},
	{"setaMaterialIndex", ( PyCFunction ) FEdgeSharp_setaMaterialIndex, METH_VARARGS, FEdgeSharp_setaMaterialIndex___doc__},
	{"setbMaterialIndex", ( PyCFunction ) FEdgeSharp_setbMaterialIndex, METH_VARARGS, FEdgeSharp_setbMaterialIndex___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdgeSharp type definition ------------------------------*/

PyTypeObject FEdgeSharp_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	FEdgeSharp___doc__,             /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
