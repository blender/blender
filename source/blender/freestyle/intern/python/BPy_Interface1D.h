#ifndef FREESTYLE_PYTHON_INTERFACE1D_H
#define FREESTYLE_PYTHON_INTERFACE1D_H

#include <Python.h>

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Interface1D_Type;

#define BPy_Interface1D_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Interface1D_Type)  )

/*---------------------------Python BPy_Interface1D structure definition----------*/
typedef struct {
	PyObject_HEAD
	Interface1D *if1D;
	int borrowed; /* non-zero if *if1D is a borrowed object */
} BPy_Interface1D;

/*---------------------------Python BPy_Interface1D visible prototypes-----------*/

int Interface1D_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTERFACE1D_H */
