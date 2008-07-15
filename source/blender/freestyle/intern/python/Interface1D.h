#ifndef FREESTYLE_PYTHON_INTERFACE1D_H
#define FREESTYLE_PYTHON_INTERFACE1D_H

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Interface1D_Type;

#define BPy_Interface1D_Check(v) \
    ((v)->ob_type == &Interface1D_Type)

/*---------------------------Python BPy_Interface1D structure definition----------*/
typedef struct {
	PyObject_HEAD
	Interface1D *if1D;
} BPy_Interface1D;

/*---------------------------Python BPy_Interface1D visible prototypes-----------*/

PyMODINIT_FUNC Interface1D_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTERFACE1D_H */
