#ifndef FREESTYLE_PYTHON_FEDGESHARP_H
#define FREESTYLE_PYTHON_FEDGESHARP_H

#include "../BPy_FEdge.h"
#include "../../../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FEdgeSharp_Type;

#define BPy_FEdgeSharp_Check(v)	(( (PyObject *) v)->ob_type == &FEdgeSharp_Type)

/*---------------------------Python BPy_FEdgeSharp structure definition----------*/
typedef struct {
	BPy_FEdge py_fe;
	FEdgeSharp *fes;
} BPy_FEdgeSharp;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FEDGESHARP_H */
