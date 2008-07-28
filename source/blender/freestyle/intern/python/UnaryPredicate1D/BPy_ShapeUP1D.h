#ifndef FREESTYLE_PYTHON_SHAPEUP1D_H
#define FREESTYLE_PYTHON_SHAPEUP1D_H

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ShapeUP1D_Type;

#define BPy_ShapeUP1D_Check(v)	(( (PyObject *) v)->ob_type == &ShapeUP1D_Type)

/*---------------------------Python BPy_ShapeUP1D structure definition----------*/
typedef struct {
	BPy_UnaryPredicate1D py_up1D;
} BPy_ShapeUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_SHAPEUP1D_H */
