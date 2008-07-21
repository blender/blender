#ifndef FREESTYLE_PYTHON_SVERTEX_H
#define FREESTYLE_PYTHON_SVERTEX_H

#include "../../view_map/Silhouette.h"
#include "../Interface0D.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SVertex_Type;

#define BPy_SVertex_Check(v)	(( (PyObject *) v)->ob_type == &SVertex_Type)

/*---------------------------Python BPy_SVertex structure definition----------*/
typedef struct {
	BPy_Interface0D py_if0D;
	SVertex *sv;
} BPy_SVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_SVERTEX_H */
