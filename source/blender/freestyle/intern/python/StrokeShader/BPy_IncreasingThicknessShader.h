#ifndef FREESTYLE_PYTHON_INCREASINGTHICKNESSSHADER_H
#define FREESTYLE_PYTHON_INCREASINGTHICKNESSSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject IncreasingThicknessShader_Type;

#define BPy_IncreasingThicknessShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &IncreasingThicknessShader_Type)  )

/*---------------------------Python BPy_IncreasingThicknessShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_IncreasingThicknessShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_INCREASINGTHICKNESSSHADER_H */
