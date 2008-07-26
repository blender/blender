#ifndef FREESTYLE_PYTHON_VIEWMAP_H
#define FREESTYLE_PYTHON_VIEWMAP_H

#include "../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewMap_Type;

#define BPy_ViewMap_Check(v)	(( (PyObject *) v)->ob_type == &ViewMap_Type)

/*---------------------------Python BPy_ViewMap structure definition----------*/
typedef struct {
	PyObject_HEAD
	ViewMap *vm;
} BPy_ViewMap;

/*---------------------------Python BPy_ViewMap visible prototypes-----------*/

PyMODINIT_FUNC ViewMap_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWMAP_H */
