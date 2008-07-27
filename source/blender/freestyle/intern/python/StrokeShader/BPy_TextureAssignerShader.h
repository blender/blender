#ifndef FREESTYLE_PYTHON_TEXTUREASSIGNERSHADER_H
#define FREESTYLE_PYTHON_TEXTUREASSIGNERSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TextureAssignerShader_Type;

#define BPy_TextureAssignerShader_Check(v)	(( (PyObject *) v)->ob_type == &TextureAssignerShader_Type)

/*---------------------------Python BPy_TextureAssignerShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_TextureAssignerShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_TEXTUREASSIGNERSHADER_H */
