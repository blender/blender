#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DVEC2F_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DVEC2F_H

#include "../BPy_UnaryFunction0D.h"

#include "../../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DVec2f_Type;

#define BPy_UnaryFunction0DVec2f_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0DVec2f_Type)

/*---------------------------Python BPy_UnaryFunction0DVec2f structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<Vec2f> *uf0D_vec2f;
} BPy_UnaryFunction0DVec2f;

/*---------------------------Python BPy_UnaryFunction0DVec2f visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DVec2f_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DVEC2F_H */
