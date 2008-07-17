#include "CurvePoint.h"

#include "../Convert.h"
#include "../../stroke/Curve.h"

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
static PyObject *CurvePoint_SetA( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_SetB( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_SetT2d( BPy_CurvePoint *self , PyObject *args);
static PyObject *CurvePoint_curvatureFredo( BPy_CurvePoint *self , PyObject *args);

/*----------------------CurvePoint instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePoint_methods[] = {	
	{"__copy__", ( PyCFunction ) CurvePoint___copy__, METH_NOARGS, "（ ）Cloning method."},
	{"A", ( PyCFunction ) CurvePoint_A, METH_NOARGS, "（ ）Returns the first SVertex upon which the CurvePoint is built."},
	{"B", ( PyCFunction ) CurvePoint_B, METH_NOARGS, "（ ）Returns the second SVertex upon which the CurvePoint is built."},
	{"t2d", ( PyCFunction ) CurvePoint_t2d, METH_NOARGS, "（ ）Returns the interpolation parameter."},
	{"SetA", ( PyCFunction ) CurvePoint_SetA, METH_VARARGS, "（SVertex sv ）Sets the first SVertex upon which to build the CurvePoint."},
	{"SetB", ( PyCFunction ) CurvePoint_SetB, METH_VARARGS, "（SVertex sv ）Sets the second SVertex upon which to build the CurvePoint."},
	{"SetT2d", ( PyCFunction ) CurvePoint_SetT2d, METH_VARARGS, "（ ）Sets the 2D interpolation parameter to use."},
	{"curvatureFredo", ( PyCFunction ) CurvePoint_curvatureFredo, METH_NOARGS, "（ ）angle in radians."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_CurvePoint type definition ------------------------------*/

PyTypeObject CurvePoint_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"CurvePoint",				/* tp_name */
	sizeof( BPy_CurvePoint ),	/* tp_basicsize */
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
	BPy_CurvePoint_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface0D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)CurvePoint___init__,                       	/* initproc tp_init; */
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

int CurvePoint___init__(BPy_CurvePoint *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

    if (! PyArg_ParseTuple(args, "|OOO", &obj1, &obj2, &obj3) )
        return -1;

	if( !obj1 && !obj2 && !obj3 ){
		self->cp = new CurvePoint();
	} else if( PyFloat_Check(obj3) ) {
		if( BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
			self->cp = new CurvePoint(  ((BPy_SVertex *) obj1)->sv,
										((BPy_SVertex *) obj2)->sv,
										PyFloat_AsDouble( obj3 ) );
		} else if( BPy_CurvePoint_Check(obj1) && BPy_CurvePoint_Check(obj2) ) {
			self->cp = new CurvePoint(  ((BPy_CurvePoint *) obj1)->cp,
										((BPy_CurvePoint *) obj2)->cp,
										PyFloat_AsDouble( obj3 ) );
		} else {
			return -1;	
		}		
	} else {
		return -1;
	}

	self->py_if0D.if0D = self->cp;

	return 0;
}

PyObject * CurvePoint___copy__( BPy_CurvePoint *self ) {
	BPy_CurvePoint *py_cp;
	
	py_cp = (BPy_CurvePoint *) CurvePoint_Type.tp_new( &CurvePoint_Type, 0, 0 );
	
	py_cp->cp = new CurvePoint( *(self->cp) );
	py_cp->py_if0D.if0D = py_cp->cp;

	return (PyObject *) py_cp;
}

PyObject * CurvePoint_A( BPy_CurvePoint *self ) {
	if( self->cp->A() )
		return BPy_SVertex_from_SVertex( *(self->cp->A()) );

	Py_RETURN_NONE;
}

PyObject * CurvePoint_B( BPy_CurvePoint *self ) {
	if( self->cp->B() )
		return BPy_SVertex_from_SVertex( *(self->cp->B()) );

	Py_RETURN_NONE;
}

PyObject * CurvePoint_t2d( BPy_CurvePoint *self ) {
	return PyFloat_FromDouble( self->cp->t2d() );
}

PyObject *CurvePoint_SetA( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv)  )) {
		cout << "ERROR: CurvePoint_SetA" << endl;
		Py_RETURN_NONE;
	}

	self->cp->SetA( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *CurvePoint_SetB( BPy_CurvePoint *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv)  )) {
		cout << "ERROR: CurvePoint_SetB" << endl;
		Py_RETURN_NONE;
	}

	self->cp->SetB( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *CurvePoint_SetT2d( BPy_CurvePoint *self , PyObject *args) {
	float t;

	if( !PyArg_ParseTuple(args, "f", &t) ) {
		cout << "ERROR: CurvePoint_SetT2d" << endl;
		Py_RETURN_NONE;
	}

	self->cp->SetT2d( t );

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
