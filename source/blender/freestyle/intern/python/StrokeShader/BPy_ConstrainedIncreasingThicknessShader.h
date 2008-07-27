#ifndef FREESTYLE_PYTHON_CONSTRAINEDINCREASINGTHICKNESSSHADER_H
#define FREESTYLE_PYTHON_CONSTRAINEDINCREASINGTHICKNESSSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ConstrainedIncreasingThicknessShader_Type;

#define BPy_ConstrainedIncreasingThicknessShader_Check(v)	(( (PyObject *) v)->ob_type == &ConstrainedIncreasingThicknessShader_Type)

/*---------------------------Python BPy_ConstrainedIncreasingThicknessShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ConstrainedIncreasingThicknessShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_CONSTRAINEDINCREASINGTHICKNESSSHADER_H */
