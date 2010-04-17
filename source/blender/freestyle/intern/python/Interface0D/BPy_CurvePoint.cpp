#include "BPy_CurvePoint.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char CurvePoint___doc__[] =
"Class to represent a point of a curve.  A CurvePoint can be any point\n"
"of a 1D curve (it doesn't have to be a vertex of the curve).  Any\n"
":class:`Interface1D` is built upon ViewEdges, themselves built upon\n"
"FEdges.  Therefore, a curve is basically a polyline made of a list of\n"
":class:`SVertex` objects.  Thus, a CurvePoint is built by linearly\n"
"interpolating two :class:`SVertex` instances.  CurvePoint can be used\n"
"as virtual points while querying 0D information along a curve at a\n"
"given resolution.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Defult constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A CurvePoint object.\n"
"   :type iBrother: :class:`CurvePoint`\n"
"\n"
".. method:: __init__(iA, iB, t2d)\n"
"\n"
"   Builds a CurvePoint from two SVertex and an interpolation parameter.\n"
"\n"
"   :arg iA: The first SVertex.\n"
"   :type iA: :class:`SVertex`\n"
"   :arg iB: The second SVertex.\n"
"   :type iB: :class:`SVertex`\n"
"   :arg t2d: A 2D interpolation parameter used to linearly interpolate\n"
"             iA and iB.\n"
"   :type t2d: float\n"
"\n"
".. method:: __init__(iA, iB, t2d)\n"
"\n"
"   Builds a CurvePoint from two CurvePoint and an interpolation\n"
"   parameter.\n"
"\n"
"   :arg iA: The first CurvePoint.\n"
"   :type iA: :class:`CurvePoint`\n"
"   :arg iB: The second CurvePoint.\n"
"   :type iB: :class:`CurvePoint`\n"
"   :arg t2d: The 2D interpolation parameter used to linearly\n"
"             interpolate iA and iB.\n"
"   :type t2d: float\n";

static int CurvePoint___init__(BPy_CurvePoint *self, PyObject *args, PyObject *kwds)
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

static PyObject * CurvePoint___copy__( BPy_CurvePoint *self ) {
	BPy_CurvePoint *py_cp;
	
	py_cp = (BPy_CurvePoint *) CurvePoint_Type.tp_new( &CurvePoint_Type, 0, 0 );
	
	py_cp->cp = new CurvePoint( *(self->cp) );
	py_cp->py_if0D.if0D = py_cp->cp;
	py_cp->py_if0D.borrowed = 0;

	return (PyObject *) py_cp;
}

static char CurvePoint_A___doc__[] =
".. method:: A()\n"
"\n"
"   Returns the first SVertex upon which the CurvePoint is built.\n"
"\n"
"   :return: The first SVertex.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * CurvePoint_A( BPy_CurvePoint *self ) {
	SVertex *A = self->cp->A();
	if( A )
		return BPy_SVertex_from_SVertex( *A );

	Py_RETURN_NONE;
}

static char CurvePoint_B___doc__[] =
".. method:: B()\n"
"\n"
"   Returns the second SVertex upon which the CurvePoint is built.\n"
"\n"
"   :return: The second SVertex.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * CurvePoint_B( BPy_CurvePoint *self ) {
	SVertex *B = self->cp->B();
	if( B )
		return BPy_SVertex_from_SVertex( *B );

	Py_RETURN_NONE;
}

static char CurvePoint_t2d___doc__[] =
".. method:: t2d()\n"
"\n"
"   Returns the 2D interpolation parameter.\n"
"\n"
"   :return: The 2D interpolation parameter.\n"
"   :rtype: float\n";

static PyObject * CurvePoint_t2d( BPy_CurvePoint *self ) {
	return PyFloat_FromDouble( self->cp->t2d() );
}

static char CurvePoint_setA___doc__[] =
".. method:: setA(iA)\n"
"\n"
"   Sets the first SVertex upon which to build the CurvePoint.\n"
"\n"
"   :arg iA: The first SVertex.\n"
"   :type iA: :class:`SVertex`\n";

static PyObject *CurvePoint_setA( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->cp->setA( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char CurvePoint_setB___doc__[] =
".. method:: setB(iB)\n"
"\n"
"   Sets the first SVertex upon which to build the CurvePoint.\n"
"\n"
"   :arg iB: The second SVertex.\n"
"   :type iB: :class:`SVertex`\n";

static PyObject *CurvePoint_setB( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->cp->setB( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char CurvePoint_setT2d___doc__[] =
".. method:: setT2d(t)\n"
"\n"
"   Sets the 2D interpolation parameter to use.\n"
"\n"
"   :arg t: The 2D interpolation parameter.\n"
"   :type t: float\n";

static PyObject *CurvePoint_setT2d( BPy_CurvePoint *self , PyObject *args) {
	float t;

	if(!( PyArg_ParseTuple(args, "f", &t) ))
		return NULL;

	self->cp->setT2d( t );

	Py_RETURN_NONE;
}

static char CurvePoint_curvatureFredo___doc__[] =
".. method:: curvatureFredo()\n"
"\n"
"   Returns the angle in radians.\n"
"\n"
"   :return: The angle in radians.\n"
"   :rtype: float\n";

static PyObject *CurvePoint_curvatureFredo( BPy_CurvePoint *self , PyObject *args) {
	return PyFloat_FromDouble( self->cp->curvatureFredo() );
}

///bool 	operator== (const CurvePoint &b)

/*----------------------CurvePoint instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePoint_methods[] = {	
	{"__copy__", ( PyCFunction ) CurvePoint___copy__, METH_NOARGS, "() Cloning method."},
	{"A", ( PyCFunction ) CurvePoint_A, METH_NOARGS, CurvePoint_A___doc__},
	{"B", ( PyCFunction ) CurvePoint_B, METH_NOARGS, CurvePoint_B___doc__},
	{"t2d", ( PyCFunction ) CurvePoint_t2d, METH_NOARGS, CurvePoint_t2d___doc__},
	{"setA", ( PyCFunction ) CurvePoint_setA, METH_VARARGS, CurvePoint_setA___doc__},
	{"setB", ( PyCFunction ) CurvePoint_setB, METH_VARARGS, CurvePoint_setB___doc__},
	{"setT2d", ( PyCFunction ) CurvePoint_setT2d, METH_VARARGS, CurvePoint_setT2d___doc__},
	{"curvatureFredo", ( PyCFunction ) CurvePoint_curvatureFredo, METH_NOARGS, CurvePoint_curvatureFredo___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_CurvePoint type definition ------------------------------*/
PyTypeObject CurvePoint_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	CurvePoint___doc__,             /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
