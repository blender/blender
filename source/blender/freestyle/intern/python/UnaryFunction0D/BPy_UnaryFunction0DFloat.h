#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DFLOAT_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DFLOAT_H

#include "../BPy_UnaryFunction0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DFloat_Type;

#define BPy_UnaryFunction0DFloat_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0DFloat_Type)

/*---------------------------Python BPy_UnaryFunction0DFloat structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<float> *uf0D_float;
} BPy_UnaryFunction0DFloat;

/*---------------------------Python BPy_UnaryFunction0DFloat visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DFloat_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DFLOAT_H */
