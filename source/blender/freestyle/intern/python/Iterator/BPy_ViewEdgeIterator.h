#ifndef FREESTYLE_PYTHON_VIEWEDGEITERATOR_H
#define FREESTYLE_PYTHON_VIEWEDGEITERATOR_H


#include "../../view_map/ViewMapIterators.h"
using namespace ViewEdgeInternal;

#include "../BPy_Iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewEdgeIterator_Type;

#define BPy_ViewEdgeIterator_Check(v)	(( (PyObject *) v)->ob_type == &ViewEdgeIterator_Type)

/*---------------------------Python BPy_ViewEdgeIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	ViewEdgeIterator *ve_it;
} BPy_ViewEdgeIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWEDGEITERATOR_H */
