#ifndef FREESTYLE_PYTHON_CHAIN_H
#define FREESTYLE_PYTHON_CHAIN_H

#include "../BPy_Curve.h"
#include "../../../stroke/Chain.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Chain_Type;

#define BPy_Chain_Check(v)	(( (PyObject *) v)->ob_type == &Chain_Type)

/*---------------------------Python BPy_Chain structure definition----------*/
typedef struct {
	BPy_Curve py_c;
	Chain *c;
} BPy_Chain;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAIN_H */
