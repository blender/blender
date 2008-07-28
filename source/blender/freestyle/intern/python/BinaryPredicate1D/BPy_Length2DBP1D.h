#ifndef FREESTYLE_PYTHON_LENGTH2DBP1D_H
#define FREESTYLE_PYTHON_LENGTH2DBP1D_H

#include "../BPy_BinaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Length2DBP1D_Type;

#define BPy_Length2DBP1D_Check(v)	(( (PyObject *) v)->ob_type == &Length2DBP1D_Type)

/*---------------------------Python BPy_Length2DBP1D structure definition----------*/
typedef struct {
	BPy_BinaryPredicate1D py_bp1D;
} BPy_Length2DBP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_LENGTH2DBP1D_H */
