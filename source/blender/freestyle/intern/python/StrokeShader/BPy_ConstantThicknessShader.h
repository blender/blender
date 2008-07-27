#ifndef FREESTYLE_PYTHON_CONSTANTTHICKNESSSHADER_H
#define FREESTYLE_PYTHON_CONSTANTTHICKNESSSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ConstantThicknessShader_Type;

#define BPy_ConstantThicknessShader_Check(v)	(( (PyObject *) v)->ob_type == &ConstantThicknessShader_Type)

/*---------------------------Python BPy_ConstantThicknessShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ConstantThicknessShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_CONSTANTTHICKNESSSHADER_H */
