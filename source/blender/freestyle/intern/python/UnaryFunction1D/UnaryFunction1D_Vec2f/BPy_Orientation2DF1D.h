#ifndef FREESTYLE_PYTHON_ORIENTATION2DF1D_H
#define FREESTYLE_PYTHON_ORIENTATION2DF1D_H

#include "../BPy_UnaryFunction1DVec2f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Orientation2DF1D_Type;

#define BPy_Orientation2DF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Orientation2DF1D_Type)  )

/*---------------------------Python BPy_Orientation2DF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVec2f py_uf1D_vec2f;
} BPy_Orientation2DF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ORIENTATION2DF1D_H */
