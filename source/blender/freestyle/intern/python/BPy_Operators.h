#ifndef FREESTYLE_PYTHON_OPERATORS_H
#define FREESTYLE_PYTHON_OPERATORS_H

#include "../stroke/Operators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Operators_Type;

#define BPy_Operators_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Operators_Type)  )

/*---------------------------Python BPy_Operators structure definition----------*/
typedef struct {
	PyObject_HEAD
} BPy_Operators;

/*---------------------------Python BPy_Operators visible prototypes-----------*/

PyMODINIT_FUNC Operators_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_OPERATORS_H */
