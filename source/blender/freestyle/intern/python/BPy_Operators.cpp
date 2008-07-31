#include "BPy_Operators.h"

#include "BPy_BinaryPredicate1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Operators instance  -----------*/
static void Operators___dealloc__(BPy_Operators *self);

static PyObject * Operators_select(BPy_Operators* self, PyObject *args);
static PyObject * Operators_bidirectionalChain(BPy_Operators* self, PyObject *args);
static PyObject * Operators_sequentialSplit(BPy_Operators* self, PyObject *args);
static PyObject * Operators_recursiveSplit(BPy_Operators* self, PyObject *args);
static PyObject * Operators_sort(BPy_Operators* self, PyObject *args);
static PyObject * Operators_create(BPy_Operators* self, PyObject *args);

/*----------------------Operators instance definitions ----------------------------*/
static PyMethodDef BPy_Operators_methods[] = {
	{"select", ( PyCFunction ) Operators_select, METH_VARARGS | METH_STATIC, 
	"select operator"},
	
	{"bidirectionalChain", ( PyCFunction ) Operators_bidirectionalChain, METH_VARARGS | METH_STATIC,
	 "select operator"},
	
	{"sequentialSplit", ( PyCFunction ) Operators_sequentialSplit, METH_VARARGS | METH_STATIC,
	 "select operator"},
	
	{"recursiveSplit", ( PyCFunction ) Operators_recursiveSplit, METH_VARARGS | METH_STATIC, 
	"select operator"},
	
	{"sort", ( PyCFunction ) Operators_sort, METH_VARARGS | METH_STATIC, 
	"select operator"},
	
	{"create", ( PyCFunction ) Operators_create, METH_VARARGS | METH_STATIC, 
	"select operator"},
	
		
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Operators type definition ------------------------------*/

PyTypeObject Operators_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Operators",				/* tp_name */
	sizeof( BPy_Operators ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Operators___dealloc__,	/* tp_dealloc */
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
	/*   Operatorss */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Operators_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	NULL,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
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
PyMODINIT_FUNC Operators_Init( PyObject *module )
{	
	if( module == NULL )
		return;

	if( PyType_Ready( &Operators_Type ) < 0 )
		return;
	Py_INCREF( &Operators_Type );
	PyModule_AddObject(module, "Operators", (PyObject *)&Operators_Type);	
	
}

//------------------------INSTANCE METHODS ----------------------------------

void Operators___dealloc__(BPy_Operators* self)
{
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * Operators_select(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if(!( 	PyArg_ParseTuple(args, "O", &obj) && 
			BPy_UnaryPredicate1D_Check(obj) && ((BPy_UnaryPredicate1D *) obj)->up1D )) {
		cout << "ERROR: Operators_select" << endl;
		Py_RETURN_NONE;
	}

	Operators::select(*( ((BPy_UnaryPredicate1D *) obj)->up1D ));

	Py_RETURN_NONE;
}

// CHANGE: first parameter is a chaining iterator, not just a view

PyObject * Operators_chain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

	if(!( 	PyArg_ParseTuple(args, "OO|O", &obj1, &obj2, &obj3) && 
			BPy_ChainingIterator_Check(obj1) && ((BPy_ChainingIterator *) obj1)->c_it &&
			BPy_UnaryPredicate1D_Check(obj2) && ((BPy_UnaryPredicate1D *) obj2)->up1D )) {
		cout << "ERROR: Operators_chain" << endl;
		Py_RETURN_NONE;
	}

	if( !obj3 ) {
		
		Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
							*( ((BPy_UnaryPredicate1D *) obj2)->up1D )  );
							
	} else if( BPy_UnaryFunction1DVoid_Check(obj3) && ((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void ) {
		
		
		Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
							*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
							*( ((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void )  );
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_bidirectionalChain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( 	PyArg_ParseTuple(args, "O|O", &obj1, &obj2) && 
			BPy_ChainingIterator_Check(obj1) && ((BPy_ChainingIterator *) obj1)->c_it )) {
		cout << "ERROR: Operators_bidirectionalChain" << endl;
		Py_RETURN_NONE;
	}
	
	if( !obj2 ) {

		Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ) );
							
	} else if( BPy_UnaryPredicate1D_Check(obj2) && ((BPy_UnaryPredicate1D *) obj2)->up1D ) {
		
		Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
										*( ((BPy_UnaryPredicate1D *) obj2)->up1D )  );
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_sequentialSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;
	float f3 = 0.0;

	if(!( 	PyArg_ParseTuple(args, "O|Of", &obj1, &obj2, &f3) && 
			BPy_UnaryPredicate0D_Check(obj1) && ((BPy_UnaryPredicate0D *) obj1)->up0D )) {
		cout << "ERROR: Operators_sequentialSplit" << endl;
		Py_RETURN_NONE;
	}
	
	if( obj2 && BPy_UnaryPredicate0D_Check(obj2) ) {
		
		Operators::sequentialSplit( 	*( ((BPy_UnaryPredicate0D *) obj1)->up0D ),
										*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
										f3 );
			
	} else {
		
		float f = ( obj2 && PyFloat_Check(obj2) ) ? PyFloat_AsDouble(obj2) : 0.0;
		
		Operators::sequentialSplit( *( ((BPy_UnaryPredicate0D *) obj1)->up0D ), f );
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_recursiveSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;
	float f4 = 0.0;

	if(!( 	PyArg_ParseTuple(args, "OO|Of", &obj1, &obj2, &obj3, &f4) && 
			BPy_UnaryFunction0DDouble_Check(obj1) && ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double )) {
		cout << "ERROR: Operators_recursiveSplit" << endl;
		Py_RETURN_NONE;
	}
	
	if( BPy_UnaryPredicate1D_Check(obj2) && ((BPy_UnaryPredicate1D *) obj2)->up1D ) {
		
		float f = ( obj3 && PyFloat_Check(obj3) ) ? PyFloat_AsDouble(obj3) : 0.0;
				
		Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
									*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
									f );
	
	} else if(	BPy_UnaryPredicate0D_Check(obj2) && ((BPy_UnaryPredicate0D *) obj2)->up0D &&
				BPy_UnaryPredicate1D_Check(obj3) && ((BPy_UnaryPredicate1D *) obj3)->up1D    ) {
		
		Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
									*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
									*( ((BPy_UnaryPredicate1D *) obj3)->up1D ),
									f4 );

	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_sort(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if(!( 	PyArg_ParseTuple(args, "O", &obj) && 
			BPy_BinaryPredicate1D_Check(obj) && ((BPy_BinaryPredicate1D *) obj)->bp1D )) {
		cout << "ERROR: Operators_sort" << endl;
		Py_RETURN_NONE;
	}

	Operators::sort(*( ((BPy_BinaryPredicate1D *) obj)->bp1D ));

	Py_RETURN_NONE;
}

PyObject * Operators_create(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( 	PyArg_ParseTuple(args, "OO", &obj1, &obj2) && 
			BPy_UnaryPredicate1D_Check(obj1) && ((BPy_UnaryPredicate1D *) obj1)->up1D &&
			PyList_Check(obj2) && PyList_Size(obj2) > 0   )) {
		cout << "ERROR: Operators_create" << endl;
		Py_RETURN_NONE;
	}

	vector<StrokeShader *> shaders;
	for( int i = 0; i < PyList_Size(obj2); i++) {
		PyObject *py_ss = PyList_GetItem(obj2,i);
		
		if( BPy_StrokeShader_Check(py_ss) )	
			shaders.push_back( ((BPy_StrokeShader *) py_ss)->ss );
	}
	
	Operators::create( *( ((BPy_UnaryPredicate1D *) obj1)->up1D ), shaders);

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


