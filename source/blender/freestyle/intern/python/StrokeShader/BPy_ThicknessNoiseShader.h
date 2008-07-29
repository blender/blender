#ifndef FREESTYLE_PYTHON_THICKNESSNOISESHADER_H
#define FREESTYLE_PYTHON_THICKNESSNOISESHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ThicknessNoiseShader_Type;

#define BPy_ThicknessNoiseShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ThicknessNoiseShader_Type)  )

/*---------------------------Python BPy_ThicknessNoiseShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ThicknessNoiseShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_THICKNESSNOISESHADER_H */
