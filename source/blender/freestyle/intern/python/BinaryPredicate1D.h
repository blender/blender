#ifndef FREESTYLE_PYTHON_BINARYPREDICATE1D_H
#define FREESTYLE_PYTHON_BINARYPREDICATE1D_H

#include "../stroke/Predicates1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BinaryPredicate1D_Type;

#define BPy_BinaryPredicate1D_Check(v)	(( (PyObject *) v)->ob_type == &BinaryPredicate1D_Type)

/*---------------------------Python BPy_BinaryPredicate1D structure definition----------*/
typedef struct {
	PyObject_HEAD
	BinaryPredicate1D *bp1D;
} BPy_BinaryPredicate1D;

/*---------------------------Python BPy_BinaryPredicate1D visible prototypes-----------*/

PyMODINIT_FUNC BinaryPredicate1D_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_BINARYPREDICATE1D_H */
