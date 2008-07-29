#ifndef FREESTYLE_PYTHON_STROKEATTRIBUTE_H
#define FREESTYLE_PYTHON_STROKEATTRIBUTE_H

#include "../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject StrokeAttribute_Type;

#define BPy_StrokeAttribute_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &StrokeAttribute_Type)  )

/*---------------------------Python BPy_StrokeAttribute structure definition----------*/
typedef struct {
	PyObject_HEAD
	StrokeAttribute *sa;
} BPy_StrokeAttribute;

/*---------------------------Python BPy_StrokeAttribute visible prototypes-----------*/

PyMODINIT_FUNC StrokeAttribute_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_STROKEATTRIBUTE_H */
