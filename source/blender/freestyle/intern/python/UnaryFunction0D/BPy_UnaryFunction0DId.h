#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DID_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DID_H

#include "../BPy_UnaryFunction0D.h"

#include "../../system/Id.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DId_Type;

#define BPy_UnaryFunction0DId_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction0DId_Type)  )

/*---------------------------Python BPy_UnaryFunction0DId structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<Id> *uf0D_id;
} BPy_UnaryFunction0DId;

/*---------------------------Python BPy_UnaryFunction0DId visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DId_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DID_H */
