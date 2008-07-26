#ifndef FREESTYLE_PYTHON_MATERIAL_H
#define FREESTYLE_PYTHON_MATERIAL_H

#include "../scene_graph/Material.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FrsMaterial_Type;

#define BPy_FrsMaterial_Check(v)	(( (PyObject *) v)->ob_type == &FrsMaterial_Type)

/*---------------------------Python BPy_FrsMaterial structure definition----------*/
typedef struct {
	PyObject_HEAD
	Material *m;
} BPy_FrsMaterial;

/*---------------------------Python BPy_FrsMaterial visible prototypes-----------*/

PyMODINIT_FUNC FrsMaterial_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_MATERIAL_H */
