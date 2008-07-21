#include "StrokeVertex.h"

#include "../../Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeVertex instance  -----------*/
static int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds);


/*----------------------StrokeVertex instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) StrokeVertex___copy__, METH_NOARGS, "（ ）Cloning method."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeVertex type definition ------------------------------*/

PyTypeObject StrokeVertex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"StrokeVertex",				/* tp_name */
	sizeof( BPy_StrokeVertex ),	/* tp_basicsize */
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
	BPy_StrokeVertex_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface0D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)StrokeVertex___init__,                       	/* initproc tp_init; */
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

int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds)
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



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


//  PyObject *_wrap_StrokeVertex_getExactTypeName(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_0(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_1(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_2(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_3(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_4(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex__SWIG_5(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_StrokeVertex(PyObject *self, PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_delete_StrokeVertex(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_x(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_y(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_getPoint(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_attribute__SWIG_0(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_attribute__SWIG_1(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_attribute(PyObject *self, PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_curvilinearAbscissa(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_strokeLength(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_u(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetX(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetY(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetPoint__SWIG_0(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetPoint__SWIG_1(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetPoint(PyObject *self, PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetAttribute(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetCurvilinearAbscissa(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_StrokeVertex_SetStrokeLength(PyObject *self , PyObject *args) {
// }
// 

