#ifndef FREESTYLE_PYTHON_FRSMATERIAL_H
#define FREESTYLE_PYTHON_FRSMATERIAL_H

#include <Python.h>

#include "../scene_graph/FrsMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FrsMaterial_Type;

#define BPy_FrsMaterial_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &FrsMaterial_Type)  )

/*---------------------------Python BPy_FrsMaterial structure definition----------*/
typedef struct {
	PyObject_HEAD
	FrsMaterial *m;
} BPy_FrsMaterial;

/*---------------------------Python BPy_FrsMaterial visible prototypes-----------*/

int FrsMaterial_Init( PyObject *module );
void FrsMaterial_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_FRSMATERIAL_H */
