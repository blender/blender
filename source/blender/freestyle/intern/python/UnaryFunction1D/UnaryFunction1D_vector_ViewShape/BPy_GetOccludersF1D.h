#ifndef FREESTYLE_PYTHON_GETOCCLUDERSF1D_H
#define FREESTYLE_PYTHON_GETOCCLUDERSF1D_H

#include "../BPy_UnaryFunction1DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetOccludersF1D_Type;

#define BPy_GetOccludersF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetOccludersF1D_Type)  )

/*---------------------------Python BPy_GetOccludersF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVectorViewShape py_uf1D_vectorviewshape;
} BPy_GetOccludersF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETOCCLUDERSF1D_H */
