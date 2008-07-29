#ifndef FREESTYLE_PYTHON_COLORNOISESHADER_H
#define FREESTYLE_PYTHON_COLORNOISESHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ColorNoiseShader_Type;

#define BPy_ColorNoiseShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ColorNoiseShader_Type)  )

/*---------------------------Python BPy_ColorNoiseShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_ColorNoiseShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_COLORNOISESHADER_H */
