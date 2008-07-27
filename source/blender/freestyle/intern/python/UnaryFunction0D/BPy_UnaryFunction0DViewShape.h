#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DVIEWSHAPE_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DVIEWSHAPE_H

#include "../BPy_UnaryFunction0D.h"

#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DViewShape_Type;

#define BPy_UnaryFunction0DViewShape_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0DViewShape_Type)

/*---------------------------Python BPy_UnaryFunction0DViewShape structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<ViewShape*> *uf0D_viewshape;
} BPy_UnaryFunction0DViewShape;

/*---------------------------Python BPy_UnaryFunction0DViewShape visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DViewShape_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DVIEWSHAPE_H */
