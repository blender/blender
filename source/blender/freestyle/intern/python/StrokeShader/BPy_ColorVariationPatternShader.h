#ifndef FREESTYLE_PYTHON_COLORVARIATIONPATTERNSHADER_H
#define FREESTYLE_PYTHON_COLORVARIATIONPATTERNSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ColorVariationPatternShader_Type;

#define BPy_ColorVariationPatternShader_Check(v)	(( (PyObject *) v)->ob_type == &ColorVariationPatternShader_Type)

/*---------------------------Python BPy_ColorVariationPatternShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ColorVariationPatternShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_COLORVARIATIONPATTERNSHADER_H */
