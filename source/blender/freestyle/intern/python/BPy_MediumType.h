#ifndef FREESTYLE_PYTHON_MEDIUMTYPE_H
#define FREESTYLE_PYTHON_MEDIUMTYPE_H

#include "../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject MediumType_Type;

#define BPy_MediumType_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &MediumType_Type)  )

/*---------------------------Python BPy_MediumType structure definition----------*/
typedef struct {
	PyIntObject i;
} BPy_MediumType;

/*---------------------------Python BPy_MediumType visible prototypes-----------*/

PyMODINIT_FUNC MediumType_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_MEDIUMTYPE_H */
