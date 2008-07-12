#ifndef FREESTYLE_PYTHON_INTERFACE0D_H
#define FREESTYLE_PYTHON_INTERFACE0D_H

#include "../view_map/Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Interface0D_Type;

#define BPy_Interface0D_Check(v) \
    ((v)->ob_type == &Interface0D_Type)

/*---------------------------Python BPy_Interface0D structure definition----------*/
typedef struct {
	PyObject_HEAD
	Interface0D *if0D;
} BPy_Interface0D;

/*---------------------------Python BPy_Interface0D visible prototypes-----------*/

PyObject *Interface0D_Init( void );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_INTERFACE0D_H */
