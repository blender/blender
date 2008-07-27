#ifndef FREESTYLE_PYTHON_CURVENATUREF1D_H
#define FREESTYLE_PYTHON_CURVENATUREF1D_H

#include "../BPy_UnaryFunction1DEdgeNature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject CurveNatureF1D_Type;

#define BPy_CurveNatureF1D_Check(v)	(( (PyObject *) v)->ob_type == &CurveNatureF1D_Type)

/*---------------------------Python BPy_CurveNatureF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DEdgeNature py_uf1D_edgenature;
} BPy_CurveNatureF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CURVENATUREF1D_H */
