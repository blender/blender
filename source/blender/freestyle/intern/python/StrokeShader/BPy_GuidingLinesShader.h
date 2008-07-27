#ifndef FREESTYLE_PYTHON_GUIDINGLINESSHADER_H
#define FREESTYLE_PYTHON_GUIDINGLINESSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GuidingLinesShader_Type;

#define BPy_GuidingLinesShader_Check(v)	(( (PyObject *) v)->ob_type == &GuidingLinesShader_Type)

/*---------------------------Python BPy_GuidingLinesShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_GuidingLinesShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_GUIDINGLINESSHADER_H */
