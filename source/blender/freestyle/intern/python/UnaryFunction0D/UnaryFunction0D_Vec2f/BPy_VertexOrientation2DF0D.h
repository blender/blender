#ifndef FREESTYLE_PYTHON_VERTEXORIENTATION2DF0D_H
#define FREESTYLE_PYTHON_VERTEXORIENTATION2DF0D_H

#include "../BPy_UnaryFunction0DVec2f.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject VertexOrientation2DF0D_Type;

#define BPy_VertexOrientation2DF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &VertexOrientation2DF0D_Type)  )

/*---------------------------Python BPy_VertexOrientation2DF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DVec2f py_uf0D_vec2f;
} BPy_VertexOrientation2DF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VERTEXORIENTATION2DF0D_H */
