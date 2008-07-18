#ifndef FREESTYLE_PYTHON_INTEGRATIONTYPE_H
#define FREESTYLE_PYTHON_INTEGRATIONTYPE_H

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject IntegrationType_Type;

#define BPy_IntegrationType_Check(v) \
    ((v)->ob_type == &IntegrationType_Type)

/*---------------------------Python BPy_IntegrationType structure definition----------*/
typedef struct {
	PyIntObject i;
} BPy_IntegrationType;

/*---------------------------Python BPy_IntegrationType visible prototypes-----------*/

PyMODINIT_FUNC IntegrationType_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTEGRATIONTYPE_H */
