#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0D_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0D_H

#include "../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0D_Type;

#define BPy_UnaryFunction0D_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0D_Type)

/*---------------------------Python BPy_UnaryFunction0D structure definition----------*/
typedef struct {
	PyObject_HEAD
	void *uf0D;
} BPy_UnaryFunction0D;

/*---------------------------Python BPy_UnaryFunction0D visible prototypes-----------*/

PyMODINIT_FUNC UnaryFunction0D_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0D_H */
