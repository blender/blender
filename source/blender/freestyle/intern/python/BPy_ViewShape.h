#ifndef FREESTYLE_PYTHON_VIEWSHAPE_H
#define FREESTYLE_PYTHON_VIEWSHAPE_H

#include <Python.h>

#include "../view_map/ViewMap.h"

#include "BPy_SShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ViewShape_Type;

#define BPy_ViewShape_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ViewShape_Type)  )

/*---------------------------Python BPy_ViewShape structure definition----------*/
typedef struct {
	PyObject_HEAD
	ViewShape *vs;
	int borrowed; /* non-zero if *vs a borrowed object */
	BPy_SShape *py_ss;
} BPy_ViewShape;

/*---------------------------Python BPy_ViewShape visible prototypes-----------*/

int ViewShape_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWSHAPE_H */
