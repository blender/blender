#ifndef FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H
#define FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H


#include "../../stroke/ChainingIterators.h"

#include "BPy_ChainingIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ChainSilhouetteIterator_Type;

#define BPy_ChainSilhouetteIterator_Check(v)	(( (PyObject *) v)->ob_type == &ChainSilhouetteIterator_Type)

/*---------------------------Python BPy_ChainSilhouetteIterator structure definition----------*/
typedef struct {
	BPy_ChainingIterator py_c_it;
	ChainSilhouetteIterator *cs_it;
} BPy_ChainSilhouetteIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H */
