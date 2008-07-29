#ifndef FREESTYLE_PYTHON_TRUEUP1D_H
#define FREESTYLE_PYTHON_TRUEUP1D_H

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TrueUP1D_Type;

#define BPy_TrueUP1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &TrueUP1D_Type)  )

/*---------------------------Python BPy_TrueUP1D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate1D py_up1D;
} BPy_TrueUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_TRUEUP1D_H */
