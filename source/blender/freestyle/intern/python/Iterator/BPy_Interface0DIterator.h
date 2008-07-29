#ifndef FREESTYLE_PYTHON_INTERFACE0DITERATOR_H
#define FREESTYLE_PYTHON_INTERFACE0DITERATOR_H

#include "../../view_map/Interface0D.h"
#include "../BPy_Iterator.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Interface0DIterator_Type;

#define BPy_Interface0DIterator_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Interface0DIterator_Type)  )

/*---------------------------Python BPy_Interface0DIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	Interface0DIterator *if0D_it;
} BPy_Interface0DIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTERFACE0DITERATOR_H */
