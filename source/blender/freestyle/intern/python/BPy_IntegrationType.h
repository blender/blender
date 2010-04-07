#ifndef FREESTYLE_PYTHON_INTEGRATIONTYPE_H
#define FREESTYLE_PYTHON_INTEGRATIONTYPE_H

#include <Python.h>

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject IntegrationType_Type;

#define BPy_IntegrationType_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &IntegrationType_Type)  )

/*---------------------------Python BPy_IntegrationType visible prototypes-----------*/

int IntegrationType_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTEGRATIONTYPE_H */
