#ifndef FREESTYLE_PYTHON_INCREMENTCHAININGTIMESTAMPF1D_H
#define FREESTYLE_PYTHON_INCREMENTCHAININGTIMESTAMPF1D_H

#include "../BPy_UnaryFunction1DVoid.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject IncrementChainingTimeStampF1D_Type;

#define BPy_IncrementChainingTimeStampF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &IncrementChainingTimeStampF1D_Type)  )

/*---------------------------Python BPy_IncrementChainingTimeStampF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DVoid py_uf1D_void;
} BPy_IncrementChainingTimeStampF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INCREMENTCHAININGTIMESTAMPF1D_H */
