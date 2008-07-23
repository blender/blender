#ifndef FREESTYLE_PYTHON_UNARYPREDICATE1D_H
#define FREESTYLE_PYTHON_UNARYPREDICATE1D_H

#include "../stroke/Predicates1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryPredicate1D_Type;

#define BPy_UnaryPredicate1D_Check(v)	(( (PyObject *) v)->ob_type == &UnaryPredicate1D_Type)

/*---------------------------Python BPy_UnaryPredicate1D structure definition----------*/
typedef struct {
	PyObject_HEAD
	UnaryPredicate1D *up1D;
} BPy_UnaryPredicate1D;

/*---------------------------Python BPy_UnaryPredicate1D visible prototypes-----------*/

PyMODINIT_FUNC UnaryPredicate1D_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_UNARYPREDICATE1D_H */
