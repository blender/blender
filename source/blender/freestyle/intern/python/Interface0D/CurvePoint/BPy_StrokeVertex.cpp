#include "BPy_StrokeVertex.h"

#include "../../BPy_Convert.h"
#include "../../BPy_StrokeAttribute.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeVertex___doc__[] =
"Class to define a stroke vertex.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A StrokeVertex object.\n"
"   :type iBrother: :class:`StrokeVertex`\n"
"\n"
".. method:: __init__(iA, iB, t3)\n"
"\n"
"   Builds a stroke vertex from 2 stroke vertices and an interpolation\n"
"   parameter.\n"
"\n"
"   :arg iA: The first StrokeVertex.\n"
"   :type iA: :class:`StrokeVertex`\n"
"   :arg iB: The second StrokeVertex.\n"
"   :type iB: :class:`StrokeVertex`\n"
"   :arg t3: An interpolation parameter.\n"
"   :type t3: float\n"
"\n"
".. method:: __init__(iPoint)\n"
"\n"
"   Builds a stroke vertex from a CurvePoint\n"
"\n"
"   :arg iPoint: A CurvePoint object.\n"
"   :type iPoint: :class:`CurvePoint`\n"
"\n"
".. method:: __init__(iSVertex)\n"
"\n"
"   Builds a stroke vertex from a SVertex\n"
"\n"
"   :arg iSVertex: An SVertex object.\n"
"   :type iSVertex: :class:`SVertex`\n"
"\n"
".. method:: __init__(iSVertex, iAttribute)\n"
"\n"
"   Builds a stroke vertex from an SVertex and a StrokeAttribute object.\n"
"\n"
"   :arg iSVertex: An SVertex object.\n"
"   :type iSVertex: :class:`SVertex`\n"
"   :arg iAttribute: A StrokeAttribute object.\n"
"   :type iAttribute: :class:`StrokeAttribute`\n";

static int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

    if (! PyArg_ParseTuple(args, "|OOO!", &obj1, &obj2, &PyFloat_Type, &obj3) )
        return -1;

	if( !obj1 ){
		self->sv = new StrokeVertex();
		
	} else if( !obj2 && BPy_StrokeVertex_Check(obj1) && ((BPy_StrokeVertex *) obj1)->sv ) {
		self->sv = new StrokeVertex( *(((BPy_StrokeVertex *) obj1)->sv) );

	} else if( !obj2 && BPy_CurvePoint_Check(obj1) && ((BPy_CurvePoint *) obj1)->cp ) {
		self->sv = new StrokeVertex( ((BPy_CurvePoint *) obj1)->cp );
	
	} else if( !obj2 && BPy_SVertex_Check(obj1) && ((BPy_SVertex *) obj1)->sv ) {
		self->sv = new StrokeVertex( ((BPy_SVertex *) obj1)->sv );
	
	} else if( obj3 && BPy_StrokeVertex_Check(obj1) && BPy_StrokeVertex_Check(obj2) ) {
		StrokeVertex *sv1 = ((BPy_StrokeVertex *) obj1)->sv;
		StrokeVertex *sv2 = ((BPy_StrokeVertex *) obj2)->sv;
		if( !sv1 || ( sv1->A() == 0 && sv1->B() == 0 ) ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid StrokeVertex object");
			return -1;
		}
		if( !sv2 || ( sv2->A() == 0 && sv2->B() == 0 ) ) {
			PyErr_SetString(PyExc_TypeError, "argument 2 is an invalid StrokeVertex object");
			return -1;
		}
		self->sv = new StrokeVertex( sv1, sv2, PyFloat_AsDouble( obj3 ) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_cp.cp = self->sv;
	self->py_cp.py_if0D.if0D = self->sv;
	self->py_cp.py_if0D.borrowed = 0;

	return 0;
}

static char StrokeVertex_x___doc__[] =
".. method:: x()\n"
"\n"
"   Returns the 2D point X coordinate.\n"
"\n"
"   :return: The X coordinate.\n"
"   :rtype: float\n";

static PyObject * StrokeVertex_x( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->x() );
}

static char StrokeVertex_y___doc__[] =
".. method:: y()\n"
"\n"
"   Returns the 2D point Y coordinate.\n"
"\n"
"   :return: The Y coordinate.\n"
"   :rtype: float\n";

static PyObject * StrokeVertex_y( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->y() );
}

static char StrokeVertex_getPoint___doc__[] =
".. method:: getPoint()\n"
"\n"
"   Returns the 2D point coordinates as a two-dimensional vector.\n"
"\n"
"   :return: The 2D coordinates.\n"
"   :rtype: :class:`Mathutils.Vector`\n";

static PyObject * StrokeVertex_getPoint( BPy_StrokeVertex *self ) {
	Vec2f v( self->sv->getPoint() );
	return Vector_from_Vec2f( v );
}

static char StrokeVertex_attribute___doc__[] =
".. method:: attribute()\n"
"\n"
"   Returns the StrokeAttribute for this StrokeVertex.\n"
"\n"
"   :return: The StrokeAttribute object.\n"
"   :rtype: :class:`StrokeAttribute`\n";

static PyObject * StrokeVertex_attribute( BPy_StrokeVertex *self ) {
	return BPy_StrokeAttribute_from_StrokeAttribute( self->sv->attribute() );
}

static char StrokeVertex_curvilinearAbscissa___doc__[] =
".. method:: curvilinearAbscissa()\n"
"\n"
"   Returns the curvilinear abscissa.\n"
"\n"
"   :return: The curvilinear abscissa.\n"
"   :rtype: float\n";

static PyObject * StrokeVertex_curvilinearAbscissa( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->curvilinearAbscissa() );
}

static char StrokeVertex_strokeLength___doc__[] =
".. method:: strokeLength()\n"
"\n"
"   Returns the length of the Stroke to which this StrokeVertex belongs\n"
"\n"
"   :return: The stroke length.\n"
"   :rtype: float\n";

static PyObject * StrokeVertex_strokeLength( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->strokeLength() );
}

static char StrokeVertex_u___doc__[] =
".. method:: u()\n"
"\n"
"   Returns the curvilinear abscissa of this StrokeVertex in the Stroke\n"
"\n"
"   :return: The curvilinear abscissa.\n"
"   :rtype: float\n";

static PyObject * StrokeVertex_u( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->u() );
}

static char StrokeVertex_setX___doc__[] =
".. method:: setX(x)\n"
"\n"
"   Sets the 2D point X coordinate.\n"
"\n"
"   :arg x: The X coordinate.\n"
"   :type x: float\n";

static PyObject *StrokeVertex_setX( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setX( r );

	Py_RETURN_NONE;
}

static char StrokeVertex_setY___doc__[] =
".. method:: setY(y)\n"
"\n"
"   Sets the 2D point Y coordinate.\n"
"\n"
"   :arg y: The Y coordinate.\n"
"   :type y: float\n";

static PyObject *StrokeVertex_setY( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setY( r );

	Py_RETURN_NONE;
}

static char StrokeVertex_setPoint___doc__[] =
".. method:: setPoint(x, y)\n"
"\n"
"   Sets the 2D point X and Y coordinates.\n"
"\n"
"   :arg x: The X coordinate.\n"
"   :type x: float\n"
"   :arg y: The Y coordinate.\n"
"   :type y: float\n"
"\n"
".. method:: SetPoint(p)\n"
"\n"
"   Sets the 2D point X and Y coordinates.\n"
"\n"
"   :arg p: A two-dimensional vector.\n"
"   :type p: :class:`Mathutils.Vector`, list or tuple of 2 real numbers\n";

static PyObject *StrokeVertex_setPoint( BPy_StrokeVertex *self , PyObject *args) {
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( PyArg_ParseTuple(args, "O|O", &obj1, &obj2) ))
		return NULL;
	
	if( obj1 && !obj2 ){
		Vec2f *v = Vec2f_ptr_from_PyObject(obj1);
		if( !v ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
			return NULL;
		}
		self->sv->setPoint( *v );
		delete v; 
	} else if( PyFloat_Check(obj1) && obj2 && PyFloat_Check(obj2) ){
		self->sv->setPoint( PyFloat_AsDouble(obj1), PyFloat_AsDouble(obj2) );
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid arguments");
		return NULL;
	}

	Py_RETURN_NONE;
}

static char StrokeVertex_setAttribute___doc__[] =
".. method:: setAttribute(iAttribute)\n"
"\n"
"   Sets the attribute.\n"
"\n"
"   :arg iAttribute: A StrokeAttribute object.\n"
"   :type iAttribute: :class:`StrokeAttribute`\n";

static PyObject *StrokeVertex_setAttribute( BPy_StrokeVertex *self , PyObject *args) {
	PyObject *py_sa;

	if(!( PyArg_ParseTuple(args, "O!", &StrokeAttribute_Type, &py_sa) ))
		return NULL;

	self->sv->setAttribute(*( ((BPy_StrokeAttribute *) py_sa)->sa ));

	Py_RETURN_NONE;
}

static char StrokeVertex_setCurvilinearAbscissa___doc__[] =
".. method:: setCurvilinearAbscissa(iAbscissa)\n"
"\n"
"   Sets the curvilinear abscissa of this StrokeVertex in the Stroke\n"
"\n"
"   :arg iAbscissa: The curvilinear abscissa.\n"
"   :type iAbscissa: float\n";

static PyObject *StrokeVertex_setCurvilinearAbscissa( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setCurvilinearAbscissa( r );

	Py_RETURN_NONE;
}

static char StrokeVertex_setStrokeLength___doc__[] =
".. method:: setStrokeLength(iLength)\n"
"\n"
"   Sets the stroke length (it is only a value retained by the\n"
"   StrokeVertex, and it won't change the real stroke length).\n"
"\n"
"   :arg iLength: The stroke length.\n"
"   :type iLength: float\n";

static PyObject *StrokeVertex_setStrokeLength( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setStrokeLength( r );

	Py_RETURN_NONE;
}

// real 	operator[] (const int i) const
// real & 	operator[] (const int i)

/*----------------------StrokeVertex instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) StrokeVertex___copy__, METH_NOARGS, "() Cloning method."},
	{"x", ( PyCFunction ) StrokeVertex_x, METH_NOARGS, StrokeVertex_x___doc__},
	{"y", ( PyCFunction ) StrokeVertex_y, METH_NOARGS, StrokeVertex_y___doc__},
	{"getPoint", ( PyCFunction ) StrokeVertex_getPoint, METH_NOARGS, StrokeVertex_getPoint___doc__},
	{"attribute", ( PyCFunction ) StrokeVertex_attribute, METH_NOARGS, StrokeVertex_attribute___doc__},
	{"curvilinearAbscissa", ( PyCFunction ) StrokeVertex_curvilinearAbscissa, METH_NOARGS, StrokeVertex_curvilinearAbscissa___doc__},
	{"strokeLength", ( PyCFunction ) StrokeVertex_strokeLength, METH_NOARGS, StrokeVertex_strokeLength___doc__},
	{"u", ( PyCFunction ) StrokeVertex_u, METH_NOARGS, StrokeVertex_u___doc__},
	{"setX", ( PyCFunction ) StrokeVertex_setX, METH_VARARGS, StrokeVertex_setX___doc__},
	{"setY", ( PyCFunction ) StrokeVertex_setY, METH_VARARGS, StrokeVertex_setY___doc__},
	{"setPoint", ( PyCFunction ) StrokeVertex_setPoint, METH_VARARGS, StrokeVertex_setPoint___doc__},
	{"setAttribute", ( PyCFunction ) StrokeVertex_setAttribute, METH_VARARGS, StrokeVertex_setAttribute___doc__},
	{"setCurvilinearAbscissa", ( PyCFunction ) StrokeVertex_setCurvilinearAbscissa, METH_VARARGS, StrokeVertex_setCurvilinearAbscissa___doc__},
	{"setStrokeLength", ( PyCFunction ) StrokeVertex_setStrokeLength, METH_VARARGS, StrokeVertex_setStrokeLength___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeVertex type definition ------------------------------*/
PyTypeObject StrokeVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeVertex",                 /* tp_name */
	sizeof(BPy_StrokeVertex),       /* tp_basicsize */
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
	StrokeVertex___doc__,           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_StrokeVertex_methods,       /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&CurvePoint_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeVertex___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
