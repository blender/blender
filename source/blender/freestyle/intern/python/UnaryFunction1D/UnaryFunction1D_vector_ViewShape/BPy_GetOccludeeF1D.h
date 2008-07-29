#ifndef FREESTYLE_PYTHON_GETOCCLUDEEF1D_H
#define FREESTYLE_PYTHON_GETOCCLUDEEF1D_H

#include "../BPy_UnaryFunction1DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetOccludeeF1D_Type;

#define BPy_GetOccludeeF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetOccludeeF1D_Type)  )

/*---------------------------Python BPy_GetOccludeeF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVectorViewShape py_uf1D_vectorviewshape;
} BPy_GetOccludeeF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETOCCLUDEEF1D_H */
