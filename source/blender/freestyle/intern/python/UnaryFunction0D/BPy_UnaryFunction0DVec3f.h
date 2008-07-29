#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DVEC3F_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DVEC3F_H

#include "../BPy_UnaryFunction0D.h"

#include "../../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DVec3f_Type;

#define BPy_UnaryFunction0DVec3f_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &UnaryFunction0DVec3f_Type)  )

/*---------------------------Python BPy_UnaryFunction0DVec3f structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<Vec3f> *uf0D_vec3f;
} BPy_UnaryFunction0DVec3f;

/*---------------------------Python BPy_UnaryFunction0DVec3f visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DVec3f_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DVEC3F_H */
