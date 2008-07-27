#ifndef FREESTYLE_PYTHON_UNARYFUNCTION1DEDGENATURE_H
#define FREESTYLE_PYTHON_UNARYFUNCTION1DEDGENATURE_H

#include "../BPy_UnaryFunction1D.h"

#include "../../winged_edge/Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction1DEdgeNature_Type;

#define BPy_UnaryFunction1DEdgeNature_Check(v)	(( (PyObject *) v)->ob_type == &UnaryFunction1DEdgeNature_Type)

/*---------------------------Python BPy_UnaryFunction1DEdgeNature structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<Nature::EdgeNature> *uf1D_edgenature;
} BPy_UnaryFunction1DEdgeNature;

/*---------------------------Python BPy_UnaryFunction1DEdgeNature visible prototypes-----------*/
PyMODINIT_FUNC UnaryFunction1DEdgeNature_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYFUNCTION1DEDGENATURE_H */
