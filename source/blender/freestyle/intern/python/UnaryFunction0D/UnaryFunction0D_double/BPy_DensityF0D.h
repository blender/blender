#ifndef FREESTYLE_PYTHON_DENSITYF0D_H
#define FREESTYLE_PYTHON_DENSITYF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject DensityF0D_Type;

#define BPy_DensityF0D_Check(v)	(( (PyObject *) v)->ob_type == &DensityF0D_Type)

/*---------------------------Python BPy_DensityF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_DensityF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_DENSITYF0D_H */
