#ifndef FREESTYLE_PYTHON_GETPROJECTEDXF1D_H
#define FREESTYLE_PYTHON_GETPROJECTEDXF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetProjectedXF1D_Type;

#define BPy_GetProjectedXF1D_Check(v)	(( (PyObject *) v)->ob_type == &GetProjectedXF1D_Type)

/*---------------------------Python BPy_GetProjectedXF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetProjectedXF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETPROJECTEDXF1D_H */
