#ifndef FREESTYLE_PYTHON_POLYGONALIZATIONSHADER_H
#define FREESTYLE_PYTHON_POLYGONALIZATIONSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject PolygonalizationShader_Type;

#define BPy_PolygonalizationShader_Check(v)	(( (PyObject *) v)->ob_type == &PolygonalizationShader_Type)

/*---------------------------Python BPy_PolygonalizationShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_PolygonalizationShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_POLYGONALIZATIONSHADER_H */
