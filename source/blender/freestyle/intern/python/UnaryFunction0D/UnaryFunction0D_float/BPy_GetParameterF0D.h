#ifndef FREESTYLE_PYTHON_GETPARAMETERF0D_H
#define FREESTYLE_PYTHON_GETPARAMETERF0D_H

#include "../BPy_UnaryFunction0DFloat.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetParameterF0D_Type;

#define BPy_GetParameterF0D_Check(v)	(( (PyObject *) v)->ob_type == &GetParameterF0D_Type)

/*---------------------------Python BPy_GetParameterF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DFloat py_uf0D_float;
} BPy_GetParameterF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETPARAMETERF0D_H */
