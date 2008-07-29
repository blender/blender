#ifndef FREESTYLE_PYTHON_CURVATURE2DANGLEF0D_H
#define FREESTYLE_PYTHON_CURVATURE2DANGLEF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Curvature2DAngleF0D_Type;

#define BPy_Curvature2DAngleF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Curvature2DAngleF0D_Type)  )

/*---------------------------Python BPy_Curvature2DAngleF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_Curvature2DAngleF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CURVATURE2DANGLEF0D_H */
