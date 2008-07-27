#ifndef FREESTYLE_PYTHON_SPATIALNOISESHADER_H
#define FREESTYLE_PYTHON_SPATIALNOISESHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SpatialNoiseShader_Type;

#define BPy_SpatialNoiseShader_Check(v)	(( (PyObject *) v)->ob_type == &SpatialNoiseShader_Type)

/*---------------------------Python BPy_SpatialNoiseShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_SpatialNoiseShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_SPATIALNOISESHADER_H */
