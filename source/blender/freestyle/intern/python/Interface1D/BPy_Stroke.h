#ifndef FREESTYLE_PYTHON_STROKE_H
#define FREESTYLE_PYTHON_STROKE_H

#include "../BPy_Interface1D.h"
#include "../../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Stroke_Type;

#define BPy_Stroke_Check(v)	(( (PyObject *) v)->ob_type == &Stroke_Type)

/*---------------------------Python BPy_Stroke structure definition----------*/
typedef struct {
	BPy_Interface1D py_if1D;
	Stroke *s;
} BPy_Stroke;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_STROKE_H */
