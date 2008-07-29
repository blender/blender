#ifndef FREESTYLE_PYTHON_TRUEUP0D_H
#define FREESTYLE_PYTHON_TRUEUP0D_H

#include "../BPy_UnaryPredicate0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TrueUP0D_Type;

#define BPy_TrueUP0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &TrueUP0D_Type)  )

/*---------------------------Python BPy_TrueUP0D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate0D py_up0D;
} BPy_TrueUP0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_TRUEUP0D_H */
