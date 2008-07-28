#ifndef FREESTYLE_PYTHON_CONTOURUP1D_H
#define FREESTYLE_PYTHON_CONTOURUP1D_H

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ContourUP1D_Type;

#define BPy_ContourUP1D_Check(v)	(( (PyObject *) v)->ob_type == &ContourUP1D_Type)

/*---------------------------Python BPy_ContourUP1D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate1D py_up1D;
} BPy_ContourUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CONTOURUP1D_H */
