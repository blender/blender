#ifndef FREESTYLE_PYTHON_ID_H
#define FREESTYLE_PYTHON_ID_H

#include <Python.h>
#include <iostream>
using namespace std;

#include "../system/Id.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Id_Type;

#define BPy_Id_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Id_Type)  )

/*---------------------------Python BPy_Id structure definition----------*/
typedef struct {
	PyObject_HEAD
	Id *id;
} BPy_Id;

/*---------------------------Python BPy_Id visible prototypes-----------*/

int Id_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_ID_H */
