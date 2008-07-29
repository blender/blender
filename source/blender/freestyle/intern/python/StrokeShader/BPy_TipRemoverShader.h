#ifndef FREESTYLE_PYTHON_TIPREMOVERSHADER_H
#define FREESTYLE_PYTHON_TIPREMOVERSHADER_H

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject TipRemoverShader_Type;

#define BPy_TipRemoverShader_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &TipRemoverShader_Type)  )

/*---------------------------Python BPy_TipRemoverShader structure definition----------*/
typedef struct {
	BPy_StrokeShader py_ss;
} BPy_TipRemoverShader;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_TIPREMOVERSHADER_H */
