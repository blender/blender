#include "BPy_Nature.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Nature instance  -----------*/
static int Nature___init__(BPy_Nature *self, PyObject *args, PyObject *kwds);

/*----------------------Nature instance definitions ----------------------------*/
static PyMethodDef BPy_Nature_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Nature type definition ------------------------------*/

PyTypeObject Nature_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Nature",				/* tp_name */
	sizeof( BPy_Nature ),	/* tp_basicsize */
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
	BPy_Nature_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&PyInt_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Nature___init__,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,			/* newfunc tp_new; */
	
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
PyMODINIT_FUNC Nature_Init( PyObject *module )
{	
	PyObject *tmp;
	
	if( module == NULL )
		return;

	if( PyType_Ready( &Nature_Type ) < 0 )
		return;
	Py_INCREF( &Nature_Type );
	PyModule_AddObject(module, "Nature", (PyObject *)&Nature_Type);

	// VertexNature
	tmp = PyInt_FromLong( Nature::POINT ); PyDict_SetItemString( Nature_Type.tp_dict, "POINT", tmp); Py_DECREF(tmp);
	tmp = PyInt_FromLong( Nature::S_VERTEX ); PyDict_SetItemString( Nature_Type.tp_dict, "S_VERTEX", tmp); Py_DECREF(tmp);
	tmp = PyInt_FromLong( Nature::VIEW_VERTEX ); PyDict_SetItemString( Nature_Type.tp_dict, "VIEW_VERTEX", tmp); Py_DECREF(tmp);
	tmp = PyInt_FromLong( Nature::NON_T_VERTEX ); PyDict_SetItemString( Nature_Type.tp_dict, "NON_T_VERTEX", tmp); Py_DECREF(tmp);
	tmp = PyInt_FromLong( Nature::T_VERTEX ); PyDict_SetItemString( Nature_Type.tp_dict, "T_VERTEX", tmp); Py_DECREF(tmp);
	tmp = PyInt_FromLong( Nature::CUSP ); PyDict_SetItemString( Nature_Type.tp_dict, "CUSP", tmp); Py_DECREF(tmp);
	
	// EdgeNature
	tmp = BPy_Nature_from_Nature( Nature::NO_FEATURE ); PyDict_SetItemString( Nature_Type.tp_dict, "NO_FEATURE", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::SILHOUETTE ); PyDict_SetItemString( Nature_Type.tp_dict, "SILHOUETTE", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::BORDER ); PyDict_SetItemString( Nature_Type.tp_dict, "BORDER", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::CREASE ); PyDict_SetItemString( Nature_Type.tp_dict, "CREASE", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::RIDGE ); PyDict_SetItemString( Nature_Type.tp_dict, "RIDGE", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::VALLEY ); PyDict_SetItemString( Nature_Type.tp_dict, "VALLEY", tmp); Py_DECREF(tmp);
	tmp = BPy_Nature_from_Nature( Nature::SUGGESTIVE_CONTOUR ); PyDict_SetItemString( Nature_Type.tp_dict, "SUGGESTIVE_CONTOUR", tmp); Py_DECREF(tmp);
	
}

int Nature___init__(BPy_Nature *self, PyObject *args, PyObject *kwds)
{
    if (PyInt_Type.tp_init((PyObject *)self, args, kwds) < 0)
        return -1;
	
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

