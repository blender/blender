#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DUNSIGNED_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DUNSIGNED_H

#include "../BPy_UnaryFunction0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DUnsigned_Type;

#define BPy_UnaryFunction0DUnsigned_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0DUnsigned_Type)

/*---------------------------Python BPy_UnaryFunction0DUnsigned structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<unsigned int> *uf0D_unsigned;
} BPy_UnaryFunction0DUnsigned;

/*---------------------------Python BPy_UnaryFunction0DUnsigned visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DUnsigned_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DUNSIGNED_H */
