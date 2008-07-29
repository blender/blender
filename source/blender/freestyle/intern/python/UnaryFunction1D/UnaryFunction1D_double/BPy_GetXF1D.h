#ifndef FREESTYLE_PYTHON_GETXF1D_H
#define FREESTYLE_PYTHON_GETXF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetXF1D_Type;

#define BPy_GetXF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetXF1D_Type)  )

/*---------------------------Python BPy_GetXF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetXF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETXF1D_H */
