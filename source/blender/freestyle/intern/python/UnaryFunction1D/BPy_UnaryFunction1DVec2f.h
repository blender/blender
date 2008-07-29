#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H

#include "../BPy_UnaryFunction1D.h"

#include "../../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DVec2f_Type;

#define BPy_UnaryFunction1DVec2f_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction1DVec2f_Type)  )

/*---------------------------Python BPy_UnaryFunction1DVec2f structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<Vec2f> *uf1D_vec2f;
} BPy_UnaryFunction1DVec2f;

/*---------------------------Python BPy_UnaryFunction1DVec2f visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DVec2f_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H */
