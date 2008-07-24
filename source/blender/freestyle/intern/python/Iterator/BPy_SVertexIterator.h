#ifndef FREESTYLE_PYTHON_SVERTEXITERATOR_H
#define FREESTYLE_PYTHON_SVERTEXITERATOR_H

#include "../../view_map/ViewMapIterators.h"

#include "../BPy_Iterator.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SVertexIterator_Type;

#define BPy_SVertexIterator_Check(v)	(( (PyObject *) v)->ob_type == &SVertexIterator_Type)

/*---------------------------Python BPy_SVertexIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	ViewEdgeInternal::SVertexIterator *sv_it;
} BPy_SVertexIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_SVERTEXITERATOR_H */
