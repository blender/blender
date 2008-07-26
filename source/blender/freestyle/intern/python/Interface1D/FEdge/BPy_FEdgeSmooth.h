#ifndef FREESTYLE_PYTHON_FEDGESMOOTH_H
#define FREESTYLE_PYTHON_FEDGESMOOTH_H

#include "../BPy_FEdge.h"
#include "../../../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FEdgeSmooth_Type;

#define BPy_FEdgeSmooth_Check(v)	(( (PyObject *) v)->ob_type == &FEdgeSmooth_Type)

/*---------------------------Python BPy_FEdgeSmooth structure definition----------*/
typedef struct {
	BPy_FEdge py_fe;
	FEdgeSmooth *fes;
} BPy_FEdgeSmooth;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FEDGESMOOTH_H */
