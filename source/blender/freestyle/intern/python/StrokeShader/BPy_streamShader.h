#ifndef FREESTYLE_PYTHON_STREAMSHADER_H
#define FREESTYLE_PYTHON_STREAMSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject streamShader_Type;

#define BPy_streamShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &streamShader_Type)  )

/*---------------------------Python BPy_streamShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_streamShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_STREAMSHADER_H */
