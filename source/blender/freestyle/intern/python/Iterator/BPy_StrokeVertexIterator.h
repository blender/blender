#ifndef FREESTYLE_PYTHON_STROKEVERTEXITERATOR_H
#define FREESTYLE_PYTHON_STROKEVERTEXITERATOR_H

#include "../../stroke/StrokeIterators.h"

#include "../BPy_Iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject StrokeVertexIterator_Type;

#define BPy_StrokeVertexIterator_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &StrokeVertexIterator_Type)  )

/*---------------------------Python BPy_StrokeVertexIterator structure definition----------*/
typedef struct {
	BPy_Iterator py_it;
	StrokeInternal::StrokeVertexIterator *sv_it;
} BPy_StrokeVertexIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_STROKEVERTEXITERATOR_H */
