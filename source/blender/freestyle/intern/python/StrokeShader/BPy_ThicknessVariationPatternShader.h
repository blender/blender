#ifndef FREESTYLE_PYTHON_THICKNESSVARIATIONPATTERNSHADER_H
#define FREESTYLE_PYTHON_THICKNESSVARIATIONPATTERNSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ThicknessVariationPatternShader_Type;

#define BPy_ThicknessVariationPatternShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ThicknessVariationPatternShader_Type)  )

/*---------------------------Python BPy_ThicknessVariationPatternShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ThicknessVariationPatternShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_THICKNESSVARIATIONPATTERNSHADER_H */
