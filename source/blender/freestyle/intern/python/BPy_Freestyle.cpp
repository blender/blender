#include "BPy_Freestyle.h"

#include "BPy_BBox.h"
#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_ContextFunctions.h"
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


//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );

/*-----------------------Freestyle module doc strings--------------------------*/

static char module_docstring[] = "The Blender Freestyle module\n\n";

/*-----------------------Freestyle module method def---------------------------*/

static PyMethodDef module_functions[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------Freestyle module definition---------------------------*/

static PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "Freestyle",
    module_docstring,
    -1,
    module_functions
};

//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Freestyle_Init( void )
{
	PyObject *module;
	
	// initialize modules
	module = PyModule_Create(&module_definition);
    if (!module)
		return NULL;
	PyDict_SetItemString(PySys_GetObject("modules"), module_definition.m_name, module);
	
	// attach its classes (adding the object types to the module)
	
	// those classes have to be initialized before the others
	MediumType_Init( module );
	Nature_Init( module );
	
	BBox_Init( module );
	BinaryPredicate0D_Init( module );
	BinaryPredicate1D_Init( module );
	ContextFunctions_Init( module );
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
