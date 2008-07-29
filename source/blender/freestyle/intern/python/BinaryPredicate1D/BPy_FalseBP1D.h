#ifndef FREESTYLE_PYTHON_FALSEBP1D_H
#define FREESTYLE_PYTHON_FALSEBP1D_H

#include "../BPy_BinaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FalseBP1D_Type;

#define BPy_FalseBP1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &FalseBP1D_Type)  )

/*---------------------------Python BPy_FalseBP1D structure definition----------*/
typedef struct {
	BPy_BinaryPredicate1D py_bp1D;
} BPy_FalseBP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FALSEBP1D_H */
