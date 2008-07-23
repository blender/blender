#ifndef FREESTYLE_PYTHON_INTERFACE0D_H
#define FREESTYLE_PYTHON_INTERFACE0D_H

#include "../view_map/Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Interface0D_Type;

#define BPy_Interface0D_Check(v)	(( (PyObject *) v)->ob_type == &Interface0D_Type)

/*---------------------------Python BPy_Interface0D structure definition----------*/
typedef struct {
	PyObject_HEAD
	Interface0D *if0D;
} BPy_Interface0D;

/*---------------------------Python BPy_Interface0D visible prototypes-----------*/

PyMODINIT_FUNC Interface0D_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTERFACE0D_H */
