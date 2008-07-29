#ifndef FREESTYLE_PYTHON_EQUALTOCHAININGTIMESTAMPUP1D_H
#define FREESTYLE_PYTHON_EQUALTOCHAININGTIMESTAMPUP1D_H

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject EqualToChainingTimeStampUP1D_Type;

#define BPy_EqualToChainingTimeStampUP1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &EqualToChainingTimeStampUP1D_Type)  )

/*---------------------------Python BPy_EqualToChainingTimeStampUP1D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate1D py_up1D;
} BPy_EqualToChainingTimeStampUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_EQUALTOCHAININGTIMESTAMPUP1D_H */
