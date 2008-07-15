#ifndef FREESTYLE_PYTHON_BINARYPREDICATE0D_H
#define FREESTYLE_PYTHON_BINARYPREDICATE0D_H

#include "../stroke/Predicates0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BinaryPredicate0D_Type;

#define BPy_BinaryPredicate0D_Check(v) \
    ((v)->ob_type == &BinaryPredicate0D_Type)

/*---------------------------Python BPy_BinaryPredicate0D structure definition----------*/
typedef struct {
	PyObject_HEAD
	BinaryPredicate0D *bp0D;
} BPy_BinaryPredicate0D;

/*---------------------------Python BPy_BinaryPredicate0D visible prototypes-----------*/

PyMODINIT_FUNC BinaryPredicate0D_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_BINARYPREDICATE0D_H */
