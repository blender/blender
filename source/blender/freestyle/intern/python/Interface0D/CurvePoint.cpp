#include "CurvePoint.h"

#include "../Convert.h"
#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for CurvePoint instance  -----------*/
static int CurvePoint___init__(BPy_CurvePoint *self, PyObject *args, PyObject *kwds);

/*----------------------CurvePoint instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePoint_methods[] = {	
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

	self->py_if0D.if0D = new CurvePoint();
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


 
// 
//  PyObject *_wrap_new_CurvePoint__SWIG_0(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_CurvePoint__SWIG_1(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_CurvePoint__SWIG_2(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_CurvePoint__SWIG_3(PyObject *self , PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_new_CurvePoint(PyObject *self, PyObject *args) {
// }
// 
// 
//  PyObject *_wrap_delete_CurvePoint(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint___eq__(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_A(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_B(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_t2d(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_SetA(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_SetB(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_SetT2d(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_fedge(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_point2d(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_point3d(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_normal(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_shape(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occluders_begin(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occluders_end(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occluders_empty(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occluders_size(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occludee(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occluded_shape(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_occludee_empty(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_z_discontinuity(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_curvatureFredo(PyObject *self , PyObject *args) {
// }
// 
// 
// PyObject *CurvePoint_directionFredo(PyObject *self , PyObject *args) {
// }


