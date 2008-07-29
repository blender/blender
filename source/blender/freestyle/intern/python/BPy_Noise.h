#ifndef FREESTYLE_PYTHON_NOISE_H
#define FREESTYLE_PYTHON_NOISE_H

#include "../geometry/Noise.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject Noise_Type;

#define BPy_Noise_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &Noise_Type)  )

/*---------------------------Python BPy_Noise structure definition----------*/
typedef struct {
	PyObject_HEAD
	Noise *n;
} BPy_Noise;

/*---------------------------Python BPy_Noise visible prototypes-----------*/

PyMODINIT_FUNC Noise_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_NOISE_H */
