#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DDOUBLE_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DDOUBLE_H

#include "../BPy_UnaryFunction0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DDouble_Type;

#define BPy_UnaryFunction0DDouble_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction0DDouble_Type)  )

/*---------------------------Python BPy_UnaryFunction0DDouble structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<double> *uf0D_double;
} BPy_UnaryFunction0DDouble;

/*---------------------------Python BPy_UnaryFunction0DDouble visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DDouble_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DDOUBLE_H */
