#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DVOID_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DVOID_H

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DVoid_Type;

#define BPy_UnaryFunction1DVoid_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction1DVoid_Type)  )

/*---------------------------Python BPy_UnaryFunction1DVoid structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<void> *uf1D_void;
} BPy_UnaryFunction1DVoid;

/*---------------------------Python BPy_UnaryFunction1DVoid visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DVoid_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DVOID_H */
