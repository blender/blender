#ifndef FREESTYLE_PYTHON_GETCOMPLETEVIEWMAPDENSITYF1D_H
#define FREESTYLE_PYTHON_GETCOMPLETEVIEWMAPDENSITYF1D_H

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetCompleteViewMapDensityF1D_Type;

#define BPy_GetCompleteViewMapDensityF1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetCompleteViewMapDensityF1D_Type)  )

/*---------------------------Python BPy_GetCompleteViewMapDensityF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetCompleteViewMapDensityF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETCOMPLETEVIEWMAPDENSITYF1D_H */
