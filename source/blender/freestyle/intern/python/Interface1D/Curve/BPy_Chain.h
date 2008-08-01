#ifndef FREESTYLE_PYTHON_CHAIN_H
#define FREESTYLE_PYTHON_CHAIN_H

#include "../BPy_FrsCurve.h"
#include "../../../stroke/Chain.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Chain_Type;

#define BPy_Chain_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Chain_Type)  )

/*---------------------------Python BPy_Chain structure definition----------*/
typedef struct {
	BPy_FrsCurve py_c;
	Chain *c;
} BPy_Chain;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAIN_H */
