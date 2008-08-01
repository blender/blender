#ifndef FREESTYLE_PYTHON_FRSCURVE_H
#define FREESTYLE_PYTHON_FRSCURVE_H

#include "../BPy_Interface1D.h"
#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject FrsCurve_Type;

#define BPy_FrsCurve_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &FrsCurve_Type)  )

/*---------------------------Python BPy_FrsCurve structure definition----------*/
typedef struct {
	BPy_Interface1D py_if1D;
	Curve *c;
} BPy_FrsCurve;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FRSCURVE_H */
