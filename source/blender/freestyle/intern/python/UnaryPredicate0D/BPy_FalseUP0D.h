#ifndef FREESTYLE_PYTHON_FALSEUP0D_H
#define FREESTYLE_PYTHON_FALSEUP0D_H

#include "../BPy_UnaryPredicate0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FalseUP0D_Type;

#define BPy_FalseUP0D_Check(v)	(( (PyObject *) v)->ob_type == &FalseUP0D_Type)

/*---------------------------Python BPy_FalseUP0D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate0D py_up0D;
} BPy_FalseUP0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FALSEUP0D_H */
