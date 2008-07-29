#ifndef FREESTYLE_PYTHON_VIEWVERTEX_H
#define FREESTYLE_PYTHON_VIEWVERTEX_H

#include "../../view_map/ViewMap.h"
#include "../BPy_Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewVertex_Type;

#define BPy_ViewVertex_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ViewVertex_Type)  )

/*---------------------------Python BPy_ViewVertex structure definition----------*/
typedef struct {
	BPy_Interface0D py_if0D;
	ViewVertex *vv;
} BPy_ViewVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWVERTEX_H */
