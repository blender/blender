#include "BPy_CurvePoint.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for CurvePoint instance  -----------*/
static int CurvePoint___init__(BPy_CurvePoint *self, PyObject *args, PyObject *kwds);
static PyObject * CurvePoint___copy__( BPy_CurvePoint *self );
static PyObject * CurvePoint_A( BPy_CurvePoint *self );
static PyObject * CurvePoint_B( BPy_CurvePoint *self );
static PyObject * CurvePoint_t2d( BPy_CurvePoint *self );
static PyObject *CurvePoint_setA( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_setB( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_setT2d( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_curvatureFredo( BPy_CurvePoint *self , PyObject *args);

/*----------------------CurvePoint instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePoint_methods[] = {	
	{"__copy__", ( PyCFunction ) CurvePoint___copy__, METH_NOARGS, "() Cloning method."},
	{"A", ( PyCFunction ) CurvePoint_A, METH_NOARGS, "() Returns the first SVertex upon which the CurvePoint is built."},
	{"B", ( PyCFunction ) CurvePoint_B, METH_NOARGS, "() Returns the second SVertex upon which the CurvePoint is built."},
	{"t2d", ( PyCFunction ) CurvePoint_t2d, METH_NOARGS, "() Returns the interpolation parameter."},
	{"setA", ( PyCFunction ) CurvePoint_setA, METH_VARARGS, "(SVertex sv) Sets the first SVertex upon which to build the CurvePoint."},
	{"setB", ( PyCFunction ) CurvePoint_setB, METH_VARARGS, "(SVertex sv) Sets the second SVertex upon which to build the CurvePoint."},
	{"setT2d", ( PyCFunction ) CurvePoint_setT2d, METH_VARARGS, "() Sets the 2D interpolation parameter to use."},
	{"curvatureFredo", ( PyCFunction ) CurvePoint_curvatureFredo, METH_NOARGS, "() angle in radians."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_CurvePoint type definition ------------------------------*/

PyTypeObject CurvePoint_Type = {
	PyObject_HEAD_INIT(NULL)
	"CurvePoint",                   /* tp_name */
	sizeof(BPy_CurvePoint),         /* tp_basicsize */
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
	"CurvePoint objects",           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_CurvePoint_methods,         /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface0D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CurvePoint___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int CurvePoint___init__(BPy_CurvePoint *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

    if (! PyArg_ParseTuple(args, "|OOO!", &obj1, &obj2, &PyFloat_Type, &obj3) )
        return -1;

	if( !obj1 ){
		self->cp = new CurvePoint();

	} else if( !obj2 && BPy_CurvePoint_Check(obj1) ) {
		self->cp = new CurvePoint( *(((BPy_CurvePoint *) obj1)->cp) );

	} else if( obj3 && BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->cp = new CurvePoint(  ((BPy_SVertex *) obj1)->sv,
									((BPy_SVertex *) obj2)->sv,
									PyFloat_AsDouble( obj3 ) );

	} else if( obj3 && BPy_CurvePoint_Check(obj1) && BPy_CurvePoint_Check(obj2) ) {
		CurvePoint *cp1 = ((BPy_CurvePoint *) obj1)->cp;
		CurvePoint *cp2 = ((BPy_CurvePoint *) obj2)->cp;
		if( !cp1 || cp1->A() == 0 || cp1->B() == 0 ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid CurvePoint object");
			return -1;
		}
		if( !cp2 || cp2->A() == 0 || cp2->B() == 0 ) {
			PyErr_SetString(PyExc_TypeError, "argument 2 is an invalid CurvePoint object");
			return -1;
		}
		self->cp = new CurvePoint( cp1, cp2, PyFloat_AsDouble( obj3 ) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_if0D.if0D = self->cp;
	self->py_if0D.borrowed = 0;

	return 0;
}

PyObject * CurvePoint___copy__( BPy_CurvePoint *self ) {
	BPy_CurvePoint *py_cp;
	
	py_cp = (BPy_CurvePoint *) CurvePoint_Type.tp_new( &CurvePoint_Type, 0, 0 );
	
	py_cp->cp = new CurvePoint( *(self->cp) );
	py_cp->py_if0D.if0D = py_cp->cp;
	py_cp->py_if0D.borrowed = 0;

	return (PyObject *) py_cp;
}

PyObject * CurvePoint_A( BPy_CurvePoint *self ) {
	SVertex *A = self->cp->A();
	if( A )
		return BPy_SVertex_from_SVertex( *A );

	Py_RETURN_NONE;
}

PyObject * CurvePoint_B( BPy_CurvePoint *self ) {
	SVertex *B = self->cp->B();
	if( B )
		return BPy_SVertex_from_SVertex( *B );

	Py_RETURN_NONE;
}

PyObject * CurvePoint_t2d( BPy_CurvePoint *self ) {
	return PyFloat_FromDouble( self->cp->t2d() );
}

PyObject *CurvePoint_setA( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->cp->setA( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *CurvePoint_setB( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->cp->setB( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *CurvePoint_setT2d( BPy_CurvePoint *self , PyObject *args) {
	float t;

	if(!( PyArg_ParseTuple(args, "f", &t) ))
		return NULL;

	self->cp->setT2d( t );

	Py_RETURN_NONE;
}

PyObject *CurvePoint_curvatureFredo( BPy_CurvePoint *self , PyObject *args) {
	return PyFloat_FromDouble( self->cp->curvatureFredo() );
}

///bool 	operator== (const CurvePoint &b)




///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
