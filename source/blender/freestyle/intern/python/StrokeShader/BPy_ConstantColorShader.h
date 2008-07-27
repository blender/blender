#ifndef FREESTYLE_PYTHON_CONSTANTCOLORSHADER_H
#define FREESTYLE_PYTHON_CONSTANTCOLORSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ConstantColorShader_Type;

#define BPy_ConstantColorShader_Check(v)	(( (PyObject *) v)->ob_type == &ConstantColorShader_Type)

/*---------------------------Python BPy_ConstantColorShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ConstantColorShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_CONSTANTCOLORSHADER_H */
