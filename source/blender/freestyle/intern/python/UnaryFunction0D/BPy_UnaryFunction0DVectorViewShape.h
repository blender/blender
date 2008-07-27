#ifndef FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H
#define FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H

#include "../BPy_UnaryFunction0D.h"

#include <vector>
#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DVectorViewShape_Type;

#define BPy_UnaryFunction0DVectorViewShape_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction0DVectorViewShape_Type)

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D< std::vector<ViewShape*> > *uf0D_vectorviewshape;
} BPy_UnaryFunction0DVectorViewShape;

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction0DVectorViewShape_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H */
