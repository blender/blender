#ifndef FREESTYLE_PYTHON_GETOCCLUDEEF0D_H
#define FREESTYLE_PYTHON_GETOCCLUDEEF0D_H

#include "../BPy_UnaryFunction0DViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetOccludeeF0D_Type;

#define BPy_GetOccludeeF0D_Check(v)	(( (PyObject *) v)->ob_type == &GetOccludeeF0D_Type)

/*---------------------------Python BPy_GetOccludeeF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DViewShape py_uf0D_viewshape;
} BPy_GetOccludeeF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETOCCLUDEEF0D_H */
