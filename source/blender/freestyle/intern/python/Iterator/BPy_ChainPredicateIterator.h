#ifndef FREESTYLE_PYTHON_CHAINPREDICATEITERATOR_H
#define FREESTYLE_PYTHON_CHAINPREDICATEITERATOR_H


#include "../../view_map/ChainingIterators.h"

#include "BPy_ChainPredicateIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ChainPredicateIterator_Type;

#define BPy_ChainPredicateIterator_Check(v)	(( (PyObject *) v)->ob_type == &ChainPredicateIterator_Type)

/*---------------------------Python BPy_ChainPredicateIterator structure definition----------*/
typedef struct {
	ChainingIterator py_c_it;
	ChainPredicateIterator *cp_it;
} BPy_ChainPredicateIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAINPREDICATEITERATOR_H */
