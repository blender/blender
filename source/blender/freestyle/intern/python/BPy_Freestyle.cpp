#include "BPy_Freestyle.h"

#include "BPy_BBox.h"
#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_FrsMaterial.h"
#include "BPy_FrsNoise.h"
#include "BPy_Id.h"
#include "BPy_IntegrationType.h"
#include "BPy_Interface0D.h"
#include "BPy_Interface1D.h"
#include "BPy_Iterator.h"
#include "BPy_MediumType.h"
#include "BPy_Nature.h"
#include "BPy_Operators.h"
#include "BPy_SShape.h"
#include "BPy_StrokeAttribute.h"
#include "BPy_StrokeShader.h"
#include "BPy_UnaryFunction0D.h"
#include "BPy_UnaryFunction1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "BPy_ViewMap.h"
#include "BPy_ViewShape.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////


/*-----------------------Python API function prototypes for the Freestyle module--*/
//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );
/*-----------------------Freestyle module doc strings-----------------------------*/
static char M_Freestyle_doc[] = "The Blender.Freestyle submodule";
/*----------------------Freestyle module method def----------------------------*/
struct PyMethodDef M_Freestyle_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Freestyle method def------------------------------*/

PyTypeObject Freestyle_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Freestyle",				/* tp_name */
	sizeof( BPy_Freestyle ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	NULL,						/* tp_repr */

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
	Py_TPFLAGS_DEFAULT, 		/* long tp_flags; */

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
	NULL,						/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,         				/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	
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
PyObject *Freestyle_Init( void )
{
	PyObject *module;
	
	if( PyType_Ready( &Freestyle_Type ) < 0 )
		return NULL;
	
	// initialize modules
	module = Py_InitModule3( "Blender.Freestyle", M_Freestyle_methods, M_Freestyle_doc );
	
	
	// attach its classes (adding the object types to the module)
	
	// those classes have to be initialized before the others
	MediumType_Init( module );
	Nature_Init( module );
	
	BBox_Init( module );
	BinaryPredicate0D_Init( module );
	BinaryPredicate1D_Init( module );
	FrsMaterial_Init( module );
	FrsNoise_Init( module );
	Id_Init( module );
	IntegrationType_Init( module );
	Interface0D_Init( module );
	Interface1D_Init( module );
	Iterator_Init( module );
	Operators_Init( module );
	SShape_Init( module );
	StrokeAttribute_Init( module );
	StrokeShader_Init( module );
	UnaryFunction0D_Init( module );
	UnaryFunction1D_Init( module );
	UnaryPredicate0D_Init( module );
	UnaryPredicate1D_Init( module );
	ViewMap_Init( module );
	ViewShape_Init( module );

	return module;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif