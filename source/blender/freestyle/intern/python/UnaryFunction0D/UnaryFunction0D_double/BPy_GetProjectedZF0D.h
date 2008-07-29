#ifndef FREESTYLE_PYTHON_GETPROJECTEDZF0D_H
#define FREESTYLE_PYTHON_GETPROJECTEDZF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetProjectedZF0D_Type;

#define BPy_GetProjectedZF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetProjectedZF0D_Type)  )

/*---------------------------Python BPy_GetProjectedZF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_GetProjectedZF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETPROJECTEDZF0D_H */
