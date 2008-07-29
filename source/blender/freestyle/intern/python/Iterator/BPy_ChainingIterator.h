#ifndef FREESTYLE_PYTHON_CHAININGITERATOR_H
#define FREESTYLE_PYTHON_CHAININGITERATOR_H


#include "../../stroke/ChainingIterators.h"

#include "BPy_ViewEdgeIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ChainingIterator_Type;

#define BPy_ChainingIterator_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ChainingIterator_Type)  )

/*---------------------------Python BPy_ChainingIterator structure definition----------*/
typedef struct {
	BPy_ViewEdgeIterator py_ve_it;
	ChainingIterator *c_it;
} BPy_ChainingIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAININGITERATOR_H */
