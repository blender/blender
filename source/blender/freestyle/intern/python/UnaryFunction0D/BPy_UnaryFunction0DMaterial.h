#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DMATERIAL_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DMATERIAL_H

#include "../BPy_UnaryFunction0D.h"

#include "../../scene_graph/Material.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DMaterial_Type;

#define BPy_UnaryFunction0DMaterial_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction0DMaterial_Type)  )

/*---------------------------Python BPy_UnaryFunction0DMaterial structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<Material> *uf0D_material;
} BPy_UnaryFunction0DMaterial;

/*---------------------------Python BPy_UnaryFunction0DMaterial visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DMaterial_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DMATERIAL_H */
