#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DVEC3F_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DVEC3F_H

#include "../BPy_UnaryFunction1D.h"

#include "../../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DVec3f_Type;

#define BPy_UnaryFunction1DVec3f_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction1DVec3f_Type)

/*---------------------------Python BPy_UnaryFunction1DVec3f structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<Vec3f> *uf1D_vec3f;
} BPy_UnaryFunction1DVec3f;

/*---------------------------Python BPy_UnaryFunction1DVec3f visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DVec3f_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DVEC3F_H */
