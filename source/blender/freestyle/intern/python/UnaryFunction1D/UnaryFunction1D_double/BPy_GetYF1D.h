#ifndef FREESTYLE_PYTHON_GETYF1D_H
#define FREESTYLE_PYTHON_GETYF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetYF1D_Type;

#define BPy_GetYF1D_Check(v)	(( (PyObject *) v)->ob_type == &GetYF1D_Type)

/*---------------------------Python BPy_GetYF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetYF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETYF1D_H */
