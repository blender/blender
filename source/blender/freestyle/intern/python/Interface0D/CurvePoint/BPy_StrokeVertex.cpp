#include "BPy_StrokeVertex.h"

#include "../../BPy_Convert.h"
#include "../../BPy_StrokeAttribute.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeVertex instance  -----------*/
static int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds);

static PyObject * StrokeVertex_x( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_y( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_getPoint( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_attribute( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_curvilinearAbscissa( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_strokeLength( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_u( BPy_StrokeVertex *self );
static PyObject * StrokeVertex_setX( BPy_StrokeVertex *self , PyObject *args);
static PyObject * StrokeVertex_setY( BPy_StrokeVertex *self , PyObject *args);
static PyObject * StrokeVertex_setPoint( BPy_StrokeVertex *self , PyObject *args);
static PyObject * StrokeVertex_setAttribute( BPy_StrokeVertex *self , PyObject *args);
static PyObject * StrokeVertex_setCurvilinearAbscissa( BPy_StrokeVertex *self , PyObject *args);
static PyObject * StrokeVertex_setStrokeLength( BPy_StrokeVertex *self , PyObject *args);

/*----------------------StrokeVertex instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) StrokeVertex___copy__, METH_NOARGS, "() Cloning method."},
	{"x", ( PyCFunction ) StrokeVertex_x, METH_NOARGS, "() Returns the 2D point x coordinate"},
	{"y", ( PyCFunction ) StrokeVertex_y, METH_NOARGS, "() Returns the 2D point y coordinate"},
	{"getPoint", ( PyCFunction ) StrokeVertex_getPoint, METH_NOARGS, "() Returns the 2D point coordinates as a Vec2d"},
	{"attribute", ( PyCFunction ) StrokeVertex_attribute, METH_NOARGS, "() Returns the StrokeAttribute of this StrokeVertex"},
	{"curvilinearAbscissa", ( PyCFunction ) StrokeVertex_curvilinearAbscissa, METH_NOARGS, "() Returns the curvilinear abscissa "},
	{"strokeLength", ( PyCFunction ) StrokeVertex_strokeLength, METH_NOARGS, "() Returns the length of the Stroke to which this StrokeVertex belongs"},
	{"u", ( PyCFunction ) StrokeVertex_u, METH_NOARGS, "() Returns the curvilinear abscissa of this StrokeVertex in the Stroke"},
	{"setX", ( PyCFunction ) StrokeVertex_setX, METH_VARARGS, "(double r) Sets the 2D x value "},
	{"setY", ( PyCFunction ) StrokeVertex_setY, METH_VARARGS, "(double r) Sets the 2D y value "},
	{"setPoint", ( PyCFunction ) StrokeVertex_setPoint, METH_VARARGS, "(double x, double y) / ( [x,y] ) Sets the 2D x and y values"},
	{"setAttribute", ( PyCFunction ) StrokeVertex_setAttribute, METH_VARARGS, "(StrokeAttribute sa) Sets the attribute."},
	{"setCurvilinearAbscissa", ( PyCFunction ) StrokeVertex_setCurvilinearAbscissa, METH_VARARGS, "(double r) Sets the curvilinear abscissa of this StrokeVertex in the Stroke"},
	{"setStrokeLength", ( PyCFunction ) StrokeVertex_setStrokeLength, METH_VARARGS, "(double r) Sets the Stroke's length (it's only a value stored by the Stroke Vertex, it won't change the real Stroke's length.) "},
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
	"StrokeVertex objects",         /* tp_doc */
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

//------------------------INSTANCE METHODS ----------------------------------

int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds)
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

PyObject * StrokeVertex_x( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->x() );
}

PyObject * StrokeVertex_y( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->y() );
}

PyObject * StrokeVertex_getPoint( BPy_StrokeVertex *self ) {
	Vec2f v( self->sv->getPoint() );
	return Vector_from_Vec2f( v );
}

PyObject * StrokeVertex_attribute( BPy_StrokeVertex *self ) {
	return BPy_StrokeAttribute_from_StrokeAttribute( self->sv->attribute() );
}

PyObject * StrokeVertex_curvilinearAbscissa( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->curvilinearAbscissa() );
}

PyObject * StrokeVertex_strokeLength( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->strokeLength() );
}

PyObject * StrokeVertex_u( BPy_StrokeVertex *self ) {
	return PyFloat_FromDouble( self->sv->u() );
}


PyObject *StrokeVertex_setX( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setX( r );

	Py_RETURN_NONE;
}

PyObject *StrokeVertex_setY( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setY( r );

	Py_RETURN_NONE;
}


PyObject *StrokeVertex_setPoint( BPy_StrokeVertex *self , PyObject *args) {
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

PyObject *StrokeVertex_setAttribute( BPy_StrokeVertex *self , PyObject *args) {
	PyObject *py_sa;

	if(!( PyArg_ParseTuple(args, "O!", &StrokeAttribute_Type, &py_sa) ))
		return NULL;

	self->sv->setAttribute(*( ((BPy_StrokeAttribute *) py_sa)->sa ));

	Py_RETURN_NONE;
}

PyObject *StrokeVertex_setCurvilinearAbscissa( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setCurvilinearAbscissa( r );

	Py_RETURN_NONE;
}


PyObject *StrokeVertex_setStrokeLength( BPy_StrokeVertex *self , PyObject *args) {
	double r;

	if(!( PyArg_ParseTuple(args, "d", &r)  ))
		return NULL;

	self->sv->setStrokeLength( r );

	Py_RETURN_NONE;
}

// real 	operator[] (const int i) const
// real & 	operator[] (const int i)

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
