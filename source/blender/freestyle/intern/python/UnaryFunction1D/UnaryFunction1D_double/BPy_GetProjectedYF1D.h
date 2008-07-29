#ifndef FREESTYLE_PYTHON_GETPROJECTEDYF1D_H
#define FREESTYLE_PYTHON_GETPROJECTEDYF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetProjectedYF1D_Type;

#define BPy_GetProjectedYF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetProjectedYF1D_Type)  )

/*---------------------------Python BPy_GetProjectedYF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetProjectedYF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETPROJECTEDYF1D_H */
