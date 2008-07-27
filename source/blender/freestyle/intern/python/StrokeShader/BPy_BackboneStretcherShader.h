#ifndef FREESTYLE_PYTHON_BACKBONESTRETCHERSHADER_H
#define FREESTYLE_PYTHON_BACKBONESTRETCHERSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BackboneStretcherShader_Type;

#define BPy_BackboneStretcherShader_Check(v)	(( (PyObject *) v)->ob_type == &BackboneStretcherShader_Type)

/*---------------------------Python BPy_BackboneStretcherShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_BackboneStretcherShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_BACKBONESTRETCHERSHADER_H */
