#ifndef FREESTYLE_PYTHON_VIEWEDGE_H
#define FREESTYLE_PYTHON_VIEWEDGE_H

#include "../../view_map/ViewMap.h"

#include "../BPy_Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewEdge_Type;

#define BPy_ViewEdge_Check(v)	(( (PyObject *) v)->ob_type == &ViewEdge_Type)

/*---------------------------Python BPy_ViewEdge structure definition----------*/
typedef struct {
	BPy_Interface1D py_if1D;
	ViewEdge *ve;
} BPy_ViewEdge;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWEDGE_H */
