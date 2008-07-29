#ifndef FREESTYLE_PYTHON_ITERATOR_H
#define FREESTYLE_PYTHON_ITERATOR_H

#include "../system/Iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Iterator_Type;

#define BPy_Iterator_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Iterator_Type)  )

/*---------------------------Python BPy_Iterator structure definition----------*/
typedef struct {
	PyObject_HEAD
	Iterator *it;
} BPy_Iterator;

/*---------------------------Python BPy_Iterator visible prototypes-----------*/

PyMODINIT_FUNC Iterator_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ITERATOR_H */
