#ifndef FREESTYLE_PYTHON_NONTVERTEX_H
#define FREESTYLE_PYTHON_NONTVERTEX_H

#include "../BPy_ViewVertex.h"
#include "../../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject NonTVertex_Type;

#define BPy_NonTVertex_Check(v)	(( (PyObject *) v)->ob_type == &NonTVertex_Type)

/*---------------------------Python BPy_NonTVertex structure definition----------*/
typedef struct {
	BPy_ViewVertex py_vv;
	NonTVertex *ntv;
} BPy_NonTVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_NONTVERTEX_H */
