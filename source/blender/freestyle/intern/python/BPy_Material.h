#ifndef FREESTYLE_PYTHON_MATERIAL_H
#define FREESTYLE_PYTHON_MATERIAL_H

#include "../scene_graph/Material.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Material_Type;

#define BPy_Material_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Material_Type)  )

/*---------------------------Python BPy_Material structure definition----------*/
typedef struct {
	PyObject_HEAD
	Material *m;
} BPy_Material;

/*---------------------------Python BPy_Material visible prototypes-----------*/

PyMODINIT_FUNC Material_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_MATERIAL_H */
