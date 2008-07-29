#ifndef FREESTYLE_PYTHON_VIEWMAPGRADIENTNORMBP1D_H
#define FREESTYLE_PYTHON_VIEWMAPGRADIENTNORMBP1D_H

#include "../BPy_BinaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewMapGradientNormBP1D_Type;

#define BPy_ViewMapGradientNormBP1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ViewMapGradientNormBP1D_Type)  )

/*---------------------------Python BPy_ViewMapGradientNormBP1D structure definition----------*/
typedef struct {
	BPy_BinaryPredicate1D py_bp1D;
} BPy_ViewMapGradientNormBP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWMAPGRADIENTNORMBP1D_H */
