#ifndef FREESTYLE_PYTHON_SAMPLINGSHADER_H
#define FREESTYLE_PYTHON_SAMPLINGSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SamplingShader_Type;

#define BPy_SamplingShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &SamplingShader_Type)  )

/*---------------------------Python BPy_SamplingShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_SamplingShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_SAMPLINGSHADER_H */
