#ifndef FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF1D_H
#define FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF1D_H

#include "../BPy_UnaryFunction1DUnsigned.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject QuantitativeInvisibilityF1D_Type;

#define BPy_QuantitativeInvisibilityF1D_Check(v)	(( (PyObject *) v)->ob_type == &QuantitativeInvisibilityF1D_Type)

/*---------------------------Python BPy_QuantitativeInvisibilityF1D structure definition----------*/
typedef struct {
	BPy_UnaryFunction1DUnsigned py_uf1D_unsigned;
} BPy_QuantitativeInvisibilityF1D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_QUANTITATIVEINVISIBILITYF1D_H */
