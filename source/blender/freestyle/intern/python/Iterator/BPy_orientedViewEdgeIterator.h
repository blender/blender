#ifndef FREESTYLE_PYTHON_ORIENTEDVIEWEDGEITERATOR_H
#define FREESTYLE_PYTHON_ORIENTEDVIEWEDGEITERATOR_H

#include "../../stroke/Stroke.h"
#include "../../view_map/ViewMapIterators.h"
using namespace ViewVertexInternal;

#include "../BPy_Iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject orientedViewEdgeIterator_Type;

#define BPy_orientedViewEdgeIterator_Check(v)	(( (PyObject *) v)->ob_type == &orientedViewEdgeIterator_Type)

/*---------------------------Python BPy_orientedViewEdgeIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	orientedViewEdgeIterator *ove_it;
} BPy_orientedViewEdgeIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ORIENTEDVIEWEDGEITERATOR_H */
