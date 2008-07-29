#ifndef FREESTYLE_PYTHON_TIMESTAMPF1D_H
#define FREESTYLE_PYTHON_TIMESTAMPF1D_H

#include "../BPy_UnaryFunction1DVoid.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TimeStampF1D_Type;

#define BPy_TimeStampF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &TimeStampF1D_Type)  )

/*---------------------------Python BPy_TimeStampF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVoid py_uf1D_void;
} BPy_TimeStampF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAININGTIMESTAMPF1D_H */
