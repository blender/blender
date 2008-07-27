#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DUNSIGNED_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DUNSIGNED_H

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DUnsigned_Type;

#define BPy_UnaryFunction1DUnsigned_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction1DUnsigned_Type)

/*---------------------------Python BPy_UnaryFunction1DUnsigned structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<unsigned int> *uf1D_unsigned;
} BPy_UnaryFunction1DUnsigned;

/*---------------------------Python BPy_UnaryFunction1DUnsigned visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DUnsigned_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DUNSIGNED_H */
