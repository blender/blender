#ifndef FREESTYLE_PYTHON_FSTREAMSHADER_H
#define FREESTYLE_PYTHON_FSTREAMSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject fstreamShader_Type;

#define BPy_fstreamShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &fstreamShader_Type)  )

/*---------------------------Python BPy_fstreamShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_fstreamShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_FSTREAMSHADER_H */
