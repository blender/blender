#ifndef FREESTYLE_PYTHON_GETZF1D_H
#define FREESTYLE_PYTHON_GETZF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetZF1D_Type;

#define BPy_GetZF1D_Check(v)	(( (PyObject *) v)->ob_type == &GetZF1D_Type)

/*---------------------------Python BPy_GetZF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetZF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETZF1D_H */
