#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DFloat_Type;

#define BPy_UnaryFunction1DFloat_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction1DFloat_Type)  )

/*---------------------------Python BPy_UnaryFunction1DFloat structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<float> *uf1D_float;
} BPy_UnaryFunction1DFloat;

/*---------------------------Python BPy_UnaryFunction1DFloat visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DFloat_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H */
