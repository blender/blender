#ifndef FREESTYLE_PYTHON_DENSITYLOWERTHANUP1D_H
#define FREESTYLE_PYTHON_DENSITYLOWERTHANUP1D_H

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject DensityLowerThanUP1D_Type;

#define BPy_DensityLowerThanUP1D_Check(v)	(( (PyObject *) v)->ob_type == &DensityLowerThanUP1D_Type)

/*---------------------------Python BPy_DensityLowerThanUP1D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate1D py_up1D;
} BPy_DensityLowerThanUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_DENSITYLOWERTHANUP1D_H */
