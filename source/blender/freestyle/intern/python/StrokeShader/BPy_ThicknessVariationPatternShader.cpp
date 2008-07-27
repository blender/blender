#include "BPy_ThicknessVariationPatternShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ThicknessVariationPatternShader instance  -----------*/
	static int ThicknessVariationPatternShader___init__( BPy_ThicknessVariationPatternShader* self, PyObject *args);

/*-----------------------BPy_ThicknessVariationPatternShader type definition ------------------------------*/

PyTypeObject ThicknessVariationPatternShader_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ThicknessVariationPatternShader",				/* tp_name */
	sizeof( BPy_ThicknessVariationPatternShader ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	NULL,										/* tp_repr */

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
	NULL,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&StrokeShader_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ThicknessVariationPatternShader___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	NULL,				/* newfunc tp_new; */
	
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

//------------------------INSTANCE METHODS ----------------------------------

int ThicknessVariationPatternShader___init__( BPy_ThicknessVariationPatternShader* self, PyObject *args)
{
	const char *s1;
	float f2 = 1.0, f3 = 5.0;
	PyObject *obj4 = 0;

	if(!( PyArg_ParseTuple(args, "s|ffO", &s1, &f2, &f3, &obj4) )) {
		cout << "ERROR: ThicknessVariationPatternShader___init__" << endl;		
		return -1;
	}

	bool b = (obj4 && PyBool_Check(obj4)) ? bool_from_PyBool(obj4) : true;
	self->py_ss.ss = new StrokeShaders::ThicknessVariationPatternShader(s1, f2, f3, b);
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
