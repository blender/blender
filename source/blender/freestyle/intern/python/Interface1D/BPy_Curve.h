#ifndef FREESTYLE_PYTHON_CURVE_H
#define FREESTYLE_PYTHON_CURVE_H

#include "../BPy_Interface1D.h"
#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Curve_Type;

#define BPy_Curve_Check(v)	(( (PyObject *) v)->ob_type == &Curve_Type)

/*---------------------------Python BPy_Curve structure definition----------*/
typedef struct {
	BPy_Interface1D py_if1D;
	Curve *c;
} BPy_Curve;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CURVE_H */
