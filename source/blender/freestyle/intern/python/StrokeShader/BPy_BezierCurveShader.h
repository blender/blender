#ifndef FREESTYLE_PYTHON_BEZIERCURVESHADER_H
#define FREESTYLE_PYTHON_BEZIERCURVESHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BezierCurveShader_Type;

#define BPy_BezierCurveShader_Check(v)	(( (PyObject *) v)->ob_type == &BezierCurveShader_Type)

/*---------------------------Python BPy_BezierCurveShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_BezierCurveShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_BEZIERCURVESHADER_H */
