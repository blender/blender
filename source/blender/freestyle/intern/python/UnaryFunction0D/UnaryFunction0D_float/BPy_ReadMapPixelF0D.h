#ifndef FREESTYLE_PYTHON_READMAPPIXELF0D_H
#define FREESTYLE_PYTHON_READMAPPIXELF0D_H

#include "../BPy_UnaryFunction0DFloat.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ReadMapPixelF0D_Type;

#define BPy_ReadMapPixelF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ReadMapPixelF0D_Type)  )

/*---------------------------Python BPy_ReadMapPixelF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DFloat py_uf0D_float;
} BPy_ReadMapPixelF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_READMAPPIXELF0D_H */
