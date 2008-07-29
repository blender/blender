#ifndef FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF0D_H
#define FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF0D_H

#include "../BPy_UnaryFunction0DUnsigned.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject QuantitativeInvisibilityF0D_Type;

#define BPy_QuantitativeInvisibilityF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &QuantitativeInvisibilityF0D_Type)  )

/*---------------------------Python BPy_QuantitativeInvisibilityF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DUnsigned py_uf0D_unsigned;
} BPy_QuantitativeInvisibilityF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF0D_H */
