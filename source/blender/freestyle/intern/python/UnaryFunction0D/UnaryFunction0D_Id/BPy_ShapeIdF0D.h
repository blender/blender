#ifndef FREESTYLE_PYTHON_SHAPEIDF0D_H
#define FREESTYLE_PYTHON_SHAPEIDF0D_H

#include "../BPy_UnaryFunction0DId.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ShapeIdF0D_Type;

#define BPy_ShapeIdF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ShapeIdF0D_Type)  )

/*---------------------------Python BPy_ShapeIdF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DId py_uf0D_id;
} BPy_ShapeIdF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_SHAPEIDF0D_H */
