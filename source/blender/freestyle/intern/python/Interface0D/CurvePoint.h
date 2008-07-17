#ifndef FREESTYLE_PYTHON_CURVEPOINT_H
#define FREESTYLE_PYTHON_CURVEPOINT_H

#include "../Interface0D.h"
#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject CurvePoint_Type;

#define BPy_CurvePoint_Check(v) \
    ((v)->ob_type == &CurvePoint_Type)

/*---------------------------Python BPy_CurvePoint structure definition----------*/
typedef struct {
	BPy_Interface0D py_if0D;
	CurvePoint *cp;
} BPy_CurvePoint;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CURVEPOINT_H */
