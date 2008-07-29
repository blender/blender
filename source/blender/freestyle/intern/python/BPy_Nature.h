#ifndef FREESTYLE_PYTHON_NATURE_H
#define FREESTYLE_PYTHON_NATURE_H

#include "../winged_edge/Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Nature_Type;

#define BPy_Nature_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Nature_Type)  )

/*---------------------------Python BPy_Nature structure definition----------*/
typedef struct {
	PyIntObject i;
} BPy_Nature;

/*---------------------------Python BPy_Nature visible prototypes-----------*/

PyMODINIT_FUNC Nature_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_NATURE_H */
