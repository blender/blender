#ifndef FREESTYLE_PYTHON_GETOCCLUDERSF0D_H
#define FREESTYLE_PYTHON_GETOCCLUDERSF0D_H

#include "../BPy_UnaryFunction0DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetOccludersF0D_Type;

#define BPy_GetOccludersF0D_Check(v)	(( (PyObject *) v)->ob_type == &GetOccludersF0D_Type)

/*---------------------------Python BPy_GetOccludersF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DVectorViewShape py_uf0D_vectorviewshape;
} BPy_GetOccludersF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETOCCLUDERSF0D_H */
