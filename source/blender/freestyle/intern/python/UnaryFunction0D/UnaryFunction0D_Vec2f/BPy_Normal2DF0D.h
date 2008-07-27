#ifndef FREESTYLE_PYTHON_NORMAL2DF0D_H
#define FREESTYLE_PYTHON_NORMAL2DF0D_H

#include "../BPy_UnaryFunction0DVec2f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Normal2DF0D_Type;

#define BPy_Normal2DF0D_Check(v)	(( (PyObject *) v)->ob_type == &Normal2DF0D_Type)

/*---------------------------Python BPy_Normal2DF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DVec2f py_uf0D_vec2f;
} BPy_Normal2DF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_NORMAL2DF0D_H */
