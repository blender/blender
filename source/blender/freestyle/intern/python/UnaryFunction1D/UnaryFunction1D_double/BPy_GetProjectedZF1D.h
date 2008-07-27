#ifndef FREESTYLE_PYTHON_GETPROJECTEDZF1D_H
#define FREESTYLE_PYTHON_GETPROJECTEDZF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetProjectedZF1D_Type;

#define BPy_GetProjectedZF1D_Check(v)	(( (PyObject *) v)->ob_type == &GetProjectedZF1D_Type)

/*---------------------------Python BPy_GetProjectedZF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetProjectedZF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETPROJECTEDZF1D_H */
