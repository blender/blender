#ifndef FREESTYLE_PYTHON_ZDISCONTINUITYF0D_H
#define FREESTYLE_PYTHON_ZDISCONTINUITYF0D_H

#include "../BPy_UnaryFunction0DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ZDiscontinuityF0D_Type;

#define BPy_ZDiscontinuityF0D_Check(v)	(( (PyObject *) v)->ob_type == &ZDiscontinuityF0D_Type)

/*---------------------------Python BPy_ZDiscontinuityF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DDouble py_uf0D_double;
} BPy_ZDiscontinuityF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ZDISCONTINUITYF0D_H */
