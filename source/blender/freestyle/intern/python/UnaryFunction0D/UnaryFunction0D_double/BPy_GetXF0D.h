#ifndef FREESTYLE_PYTHON_GETXF0D_H
#define FREESTYLE_PYTHON_GETXF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetXF0D_Type;

#define BPy_GetXF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetXF0D_Type)  )

/*---------------------------Python BPy_GetXF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_GetXF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETXF0D_H */
