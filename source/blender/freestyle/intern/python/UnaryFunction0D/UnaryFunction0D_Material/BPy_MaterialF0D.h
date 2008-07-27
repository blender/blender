#ifndef FREESTYLE_PYTHON_MATERIALF0D_H
#define FREESTYLE_PYTHON_MATERIALF0D_H

#include "../BPy_UnaryFunction0DMaterial.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject MaterialF0D_Type;

#define BPy_MaterialF0D_Check(v)	(( (PyObject *) v)->ob_type == &MaterialF0D_Type)

/*---------------------------Python BPy_MaterialF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DMaterial py_uf0D_material;
} BPy_MaterialF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_MATERIALF0D_H */
