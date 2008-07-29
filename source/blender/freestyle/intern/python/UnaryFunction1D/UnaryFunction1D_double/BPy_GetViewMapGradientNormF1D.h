#ifndef FREESTYLE_PYTHON_GETVIEWMAPGRADIENTNORMF1D_H
#define FREESTYLE_PYTHON_GETVIEWMAPGRADIENTNORMF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetViewMapGradientNormF1D_Type;

#define BPy_GetViewMapGradientNormF1D_Check(v)	( ((PyObject *) v)->ob_type == PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetViewMapGradientNormF1D_Type)  )

/*---------------------------Python BPy_GetViewMapGradientNormF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetViewMapGradientNormF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETVIEWMAPGRADIENTNORMF1D_H */
