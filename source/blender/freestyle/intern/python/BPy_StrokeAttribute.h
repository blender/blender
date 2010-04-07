#ifndef FREESTYLE_PYTHON_STROKEATTRIBUTE_H
#define FREESTYLE_PYTHON_STROKEATTRIBUTE_H

#include <Python.h>

#include "../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject StrokeAttribute_Type;

#define BPy_StrokeAttribute_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &StrokeAttribute_Type)  )

/*---------------------------Python BPy_StrokeAttribute structure definition----------*/
typedef struct {
	PyObject_HEAD
	StrokeAttribute *sa;
	int borrowed; /* non-zero if *sa is a borrowed reference */
} BPy_StrokeAttribute;

/*---------------------------Python BPy_StrokeAttribute visible prototypes-----------*/

int StrokeAttribute_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_STROKEATTRIBUTE_H */
