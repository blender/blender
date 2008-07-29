#ifndef FREESTYLE_PYTHON_GETCURVILINEARABSCISSAF0D_H
#define FREESTYLE_PYTHON_GETCURVILINEARABSCISSAF0D_H

#include "../BPy_UnaryFunction0DFloat.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject GetCurvilinearAbscissaF0D_Type;

#define BPy_GetCurvilinearAbscissaF0D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &GetCurvilinearAbscissaF0D_Type)  )

/*---------------------------Python BPy_GetCurvilinearAbscissaF0D structure definition----------*/
typedef struct {
	BPy_UnaryFunction0DFloat py_uf0D_float;
} BPy_GetCurvilinearAbscissaF0D;


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_GETCURVILINEARABSCISSAF0D_H */
