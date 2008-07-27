#ifndef FREESTYLE_PYTHON_CURVENATUREF0D_H
#define FREESTYLE_PYTHON_CURVENATUREF0D_H

#include "../BPy_UnaryFunction0DEdgeNature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject CurveNatureF0D_Type;

#define BPy_CurveNatureF0D_Check(v)	(( (PyObject *) v)->ob_type == &CurveNatureF0D_Type)

/*---------------------------Python BPy_CurveNatureF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DEdgeNature py_uf0D_edgenature;
} BPy_CurveNatureF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CURVENATUREF0D_H */
