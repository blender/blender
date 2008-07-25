#ifndef FREESTYLE_PYTHON_TVERTEX_H
#define FREESTYLE_PYTHON_TVERTEX_H

#include "../BPy_ViewVertex.h"
#include "../../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TVertex_Type;

#define BPy_TVertex_Check(v)	(( (PyObject *) v)->ob_type == &TVertex_Type)

/*---------------------------Python BPy_TVertex structure definition----------*/
typedef struct {
	BPy_ViewVertex py_vv;
	TVertex *tv;
} BPy_TVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_TVERTEX_H */
