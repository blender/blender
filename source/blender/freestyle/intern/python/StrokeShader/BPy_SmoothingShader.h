#ifndef FREESTYLE_PYTHONSMOOTHINGSHADER_H
#define FREESTYLE_PYTHONSMOOTHINGSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SmoothingShader_Type;

#define BPy_SmoothingShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &SmoothingShader_Type)  )

/*---------------------------Python BPy_SmoothingShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_SmoothingShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHONSMOOTHINGSHADER_H */
