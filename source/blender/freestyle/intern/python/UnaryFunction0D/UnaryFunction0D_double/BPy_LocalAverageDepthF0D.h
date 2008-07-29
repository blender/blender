#ifndef FREESTYLE_PYTHON_LOCALAVERAGEDEPTHF0D_H
#define FREESTYLE_PYTHON_LOCALAVERAGEDEPTHF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject LocalAverageDepthF0D_Type;

#define BPy_LocalAverageDepthF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &LocalAverageDepthF0D_Type)  )

/*---------------------------Python BPy_LocalAverageDepthF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_LocalAverageDepthF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_LOCALAVERAGEDEPTHF0D_H */
