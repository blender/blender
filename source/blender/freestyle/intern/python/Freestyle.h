#ifndef FREESTYLE_PYTHON_FREESTYLE_H
#define FREESTYLE_PYTHON_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Freestyle_Type;

#define BPy_Freestyle_Check(v) \
    ((v)->ob_type == &Freestyle_Type)

/*---------------------------Python BPy_Freestyle structure definition----------*/
typedef struct {
	PyObject_HEAD 
} BPy_Freestyle;

/*---------------------------Python BPy_Freestyle visible prototypes-----------*/

PyObject *Freestyle_Init( void );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_FREESTYLE_H */
