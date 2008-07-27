#ifndef FREESTYLE_PYTHON_GETSHAPEF1D_H
#define FREESTYLE_PYTHON_GETSHAPEF1D_H

#include "../BPy_UnaryFunction1DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetShapeF1D_Type;

#define BPy_GetShapeF1D_Check(v)	(( (PyObject *) v)->ob_type == &GetShapeF1D_Type)

/*---------------------------Python BPy_GetShapeF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVectorViewShape py_uf1D_vectorviewshape;
} BPy_GetShapeF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETSHAPEF1D_H */
