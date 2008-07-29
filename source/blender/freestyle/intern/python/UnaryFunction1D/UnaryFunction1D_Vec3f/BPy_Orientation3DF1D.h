#ifndef FREESTYLE_PYTHON_ORIENTATION3DF1D_H
#define FREESTYLE_PYTHON_ORIENTATION3DF1D_H

#include "../BPy_UnaryFunction1DVec3f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Orientation3DF1D_Type;

#define BPy_Orientation3DF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Orientation3DF1D_Type)  )

/*---------------------------Python BPy_Orientation3DF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVec3f py_uf1D_vec3f;
} BPy_Orientation3DF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ORIENTATION3DF1D_H */
