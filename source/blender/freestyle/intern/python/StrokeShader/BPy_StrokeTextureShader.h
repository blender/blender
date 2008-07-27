#ifndef FREESTYLE_PYTHON_STROKETEXTURESHADER_H
#define FREESTYLE_PYTHON_STROKETEXTURESHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject StrokeTextureShader_Type;

#define BPy_StrokeTextureShader_Check(v)	(( (PyObject *) v)->ob_type == &StrokeTextureShader_Type)

/*---------------------------Python BPy_StrokeTextureShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_StrokeTextureShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_STROKETEXTURESHADER_H */
