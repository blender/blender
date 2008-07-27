#ifndef FREESTYLE_PYTHON_INCREASINGCOLORSHADER_H
#define FREESTYLE_PYTHON_INCREASINGCOLORSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject IncreasingColorShader_Type;

#define BPy_IncreasingColorShader_Check(v)	(( (PyObject *) v)->ob_type == &IncreasingColorShader_Type)

/*---------------------------Python BPy_IncreasingColorShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_IncreasingColorShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_INCREASINGCOLORSHADER_H */
