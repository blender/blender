#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DDOUBLE_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DDOUBLE_H

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DDouble_Type;

#define BPy_UnaryFunction1DDouble_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction1DDouble_Type)  )

/*---------------------------Python BPy_UnaryFunction1DDouble structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<double> *uf1D_double;
} BPy_UnaryFunction1DDouble;

/*---------------------------Python BPy_UnaryFunction1DDouble visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DDouble_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DDOUBLE_H */
